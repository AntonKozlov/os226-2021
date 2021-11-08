#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"
#include "ctx.h"
#include "syscall.h"

/* AMD64 Sys V ABI, 3.2.2 The Stack Frame:
   The 128-byte area beyond the location pointed to by %rsp is considered to
   be reserved and shall not be modified by signal or interrupt handlers */
#define SYSV_REDST_SZ 128

#define TICK_PERIOD 100

#define MEM_PAGES 1024 // The number of pages in the physical memory
#define PAGE_SIZE 4096 // Page size in bytes

#define USER_PAGES 1024 // The number of pages in the virtual memory
#define USER_START ((void*)0x400000) // Starting address of the virtual memory
#define USER_STACK_PAGES 2 // The number of pages occupied by the stack in the virtual memory

extern int shell(int argc, char* argv[]);

extern void tramptramp(void);

struct vmctx {
    unsigned int map[USER_PAGES];
};

struct task {
    char stack[8192];
    struct ctx ctx;
    struct vmctx vm;

    void (* entry)(void* as);

    void* as;
    int priority;

    // timeout support
    unsigned int waketime;

    // policy support
    struct task* next;
};

static int time = 0;

static unsigned int current_start;
static struct task* current;
static struct task* idle;
static struct task* runq;
static struct task* waitq;

static struct task* pendingq;
static struct task* lastpending;

static int (* policy_cmp)(struct task* t1, struct task* t2);

static struct task taskarray[16];
static struct pool taskpool = POOL_INITIALIZER_ARRAY(taskarray);

static sigset_t irqs;

static int memfd = -1; // A file for virtual memory allocation
static unsigned long bitmap_pages[MEM_PAGES / (sizeof(unsigned long) *
                                               CHAR_BIT)]; // Bitmap representing the allocation status of the memory pages

void irq_disable(void) {
    if (sigprocmask(SIG_BLOCK, &irqs, NULL) == -1) fprintf(stderr, "Failed to disable timer IRQ");
}

void irq_enable(void) {
    if (sigprocmask(SIG_UNBLOCK, &irqs, NULL) == -1) fprintf(stderr, "Failed to enable timer IRQ");
}

// Marks a page in the memfd as allocated and returns a pointer to it
static int bitmap_alloc(unsigned long* bitmap, size_t size) {
    size_t n = size / sizeof(*bitmap);
    unsigned long* bitmap_elem = NULL;

    for (size_t i = 0; i < n; i++) {
        if (bitmap[i] != -1) {
            bitmap_elem = &bitmap[i];
            break;
        }
    }

    if (bitmap_elem == NULL) return -1;

    int bit_num = ffsl((long) *bitmap_elem + 1) - 1;
    *bitmap_elem |= 1 << bit_num;

    return (int) ((bitmap_elem - bitmap) * sizeof(unsigned long) * CHAR_BIT) + bit_num;
}

// Adds the task to the run queue
static void policy_run(struct task* t) {
    struct task** c = &runq;

    while (*c != NULL && (t == idle || policy_cmp(*c, t) <= 0)) c = &((*c)->next);
    t->next = *c;
    *c = t;
}

static void vmctx_make(struct vmctx* vm, size_t stack_size) {
    memset(vm->map, -1, sizeof(vm->map));
    for (int i = 0; i < stack_size / PAGE_SIZE; i++) {
        int mempage = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
        if (mempage == -1) {
            fprintf(stderr, "bitmap_alloc failed");
            abort();
        }
        vm->map[USER_PAGES - 1 - i] = mempage;
    }
}

// Applies the virtual memory mapping (switches the context)
static void vmctx_apply(struct vmctx* vm) {
    if (munmap(USER_START, USER_STACK_PAGES * PAGE_SIZE) == -1) { // TODO: fix len, use MAP_FIXED_NOREPLACE in mmap
        perror("munmap");
        abort();
    }

    for (int i = 0; i < USER_PAGES; i++) {
        if (vm->map[i] == -1) continue;

        if (mmap(USER_START + i * PAGE_SIZE, PAGE_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED, memfd, vm->map[i] * PAGE_SIZE) == MAP_FAILED) {
            perror("mmap");
            abort();
        }
    }
}

static void doswitch(void) {
    struct task* old = current;
    current = runq;
    runq = current->next;

    current_start = sched_gettime();
    vmctx_apply(&current->vm);
    ctx_switch(&old->ctx, &current->ctx);
}

// Runs the current task
static void tasktramp(void) {
    irq_enable();
    current->entry(current->as);
    irq_disable();
    doswitch();
}

static void tasktramp0(void) {
    struct ctx dummy, new;
    vmctx_apply(&current->vm);
    ctx_make(&new, tasktramp, USER_START + (USER_PAGES - USER_STACK_PAGES) * PAGE_SIZE,
             USER_STACK_PAGES * PAGE_SIZE);
    ctx_switch(&dummy, &new);
}

// Creates context for the task and adds it to the pending queue
void sched_new(void (* entrypoint)(void* aspace), void* aspace, int priority) {
    struct task* t = pool_alloc(&taskpool);
    *t = (struct task) {.entry = entrypoint, .as = aspace, .priority = priority, .next = NULL};

    vmctx_make(&t->vm, 8192);
    ctx_make(&t->ctx, tasktramp0, t->stack, sizeof(t->stack));

    if (lastpending == NULL) {
        lastpending = t;
        pendingq = t;
    } else {
        lastpending->next = t;
        lastpending = t;
    }
}

void sched_sleep(unsigned ms) {
    if (ms == 0) {
        irq_disable();
        policy_run(current);
        doswitch();
        irq_enable();
        return;
    }

    current->waketime = sched_gettime() + ms;

    unsigned int curtime;
    while ((curtime = sched_gettime()) < current->waketime) {
        irq_disable();
        struct task** c = &waitq;
        while (*c != NULL && (*c)->waketime < current->waketime) c = &(*c)->next;
        current->next = *c;
        *c = current;

        doswitch();
        irq_enable();
    }
}

static int fifo_cmp(struct task* t1, struct task* t2) {
    return -1;
}

static int prio_cmp(struct task* t1, struct task* t2) {
    return t2->priority - t1->priority;
}

static void hctx_push(greg_t* regs, unsigned long val) {
    regs[REG_RSP] -= sizeof(unsigned long);
    *(unsigned long*) regs[REG_RSP] = val;
}

static void bottom(void) {
    time += TICK_PERIOD;

    while (waitq != NULL && waitq->waketime <= sched_gettime()) {
        struct task* t = waitq;
        waitq = waitq->next;
        irq_disable();
        policy_run(t);
        irq_enable();
    }

    if (TICK_PERIOD <= sched_gettime() - current_start) {
        irq_disable();
        policy_run(current);
        doswitch();
        irq_enable();
    }
}

static void top(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* uc = (ucontext_t*) ctx;
    greg_t* regs = uc->uc_mcontext.gregs;

    unsigned long oldsp = regs[REG_RSP];
    regs[REG_RSP] -= SYSV_REDST_SZ;
    hctx_push(regs, regs[REG_RIP]);
    hctx_push(regs, oldsp);
    hctx_push(regs, (unsigned long) (current->stack + sizeof(current->stack) - 16));
    hctx_push(regs, (unsigned long) bottom);
    regs[REG_RIP] = (greg_t) tramptramp;
}

long sched_gettime(void) {
    return time + timer_cnt() / 1000;
}

void sched_run(enum policy policy) {
    int (* policies[])(struct task* t1, struct task* t2) = {fifo_cmp, prio_cmp};
    policy_cmp = policies[policy];

    struct task* t = pendingq;
    while (t != NULL) {
        struct task* next = t->next;
        policy_run(t);
        t = next;
    }

    sigemptyset(&irqs);
    sigaddset(&irqs, SIGALRM);

    timer_init(TICK_PERIOD, top);

    irq_disable();

    idle = pool_alloc(&taskpool);
    memset(&idle->vm.map, -1, sizeof(idle->vm.map));

    current = idle;

    sigset_t none;
    sigemptyset(&none);

    while (runq != NULL || waitq != NULL) {
        if (runq != NULL) {
            policy_run(current);
            doswitch();
        } else sigsuspend(&none);
    }

    irq_enable();
}

static void sighnd(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* uc = (ucontext_t*) ctx;
    greg_t* regs = uc->uc_mcontext.gregs;

    uint16_t insn = *(uint16_t*) regs[REG_RIP];
    if (insn != 0x81cd) {
        fprintf(stderr, "sighnd: received %x\n", insn);
        abort();
    }

    regs[REG_RAX] = (greg_t) syscall_do((int) regs[REG_RAX], regs[REG_RBX],
                                        regs[REG_RCX], regs[REG_RDX],
                                        regs[REG_RSI], (void*) regs[REG_RDI]);

    regs[REG_RIP] += 2;
}

int main(int argc, char* argv[]) {
    struct sigaction act = {
            .sa_sigaction = sighnd,
            .sa_flags = SA_RESTART,
    };
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGSEGV, &act, NULL) == -1) {
        perror("signal set failed");
        return 1;
    }

    memfd = memfd_create("mem", 0);
    if (memfd == -1) {
        perror("memfd_create");
        return 1;
    }

    if (ftruncate(memfd, PAGE_SIZE * MEM_PAGES) == -1) {
        perror("ftrucate");
        return 1;
    }

    shell(0, NULL);
}
