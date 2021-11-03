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

#define MEM_PAGES 1024
#define PAGE_SIZE 4096

#define USER_PAGES 1024
#define USER_START ((void*)0x400000)
#define USER_STACK_PAGES 2

extern int shell(int argc, char *argv[]);

extern void tramptramp(void);

struct vmctx {
    unsigned map[USER_PAGES];
};

struct task {
    char stack[8192];
    struct ctx ctx;
    struct vmctx vm;

    void (*entry)(void *as);

    void *as;
    int priority;

    // timeout support
    int waketime;

    // policy support
    struct task *next;
};

static int time;

static int current_start;
static struct task *current;
static struct task *idle;
static struct task *runq;
static struct task *waitq;

static struct task *pendingq;
static struct task *lastpending;

static int (*policy_cmp)(struct task *t1, struct task *t2);

static struct task taskarray[16];
static struct pool taskpool = POOL_INITIALIZER_ARRAY(taskarray);

static sigset_t irqs;

static int memfd = -1;
static unsigned long bitmap_pages[MEM_PAGES / (sizeof(unsigned long) * CHAR_BIT)];

void irq_disable(void) {
    sigprocmask(SIG_BLOCK, &irqs, NULL);
}

void irq_enable(void) {
    sigprocmask(SIG_UNBLOCK, &irqs, NULL);
}

static int bitmap_alloc(unsigned long *bitmap, size_t size) {
    unsigned long bit = ((unsigned long) 1) << ((sizeof(unsigned long) * CHAR_BIT) - 1);
    for (int i = 0; i < size; i++) {
        for (int off = 0; off < ((sizeof(unsigned long) * CHAR_BIT) - 1); off++) {
            if ((bit & bitmap[i]) == 0) {
                bitmap[i] |= bit;
                return i * (sizeof(unsigned long) * CHAR_BIT) + off;
            }
            bit /= 2;
        }
    }
    return -1;
}

static void policy_run(struct task *t) {
    struct task **c = &runq;

    while (*c && (t == idle || policy_cmp(*c, t) <= 0)) {
        c = &(*c)->next;
    }
    t->next = *c;
    *c = t;
}

static void vmctx_make(struct vmctx *vm, size_t stack_size) {
    memset(vm->map, -1, sizeof(vm->map));
    for (int i = 0; i < stack_size / PAGE_SIZE; ++i) {
        int mempage = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
        if (mempage == -1) {
            abort();
        }
        vm->map[USER_PAGES - 1 - i] = mempage;
    }
}

static void vmctx_apply(struct vmctx *vm) {
    for (int i = 0; i < USER_PAGES; i++) {
        if (vm->map[i] != -1) {
            munmap(USER_START + i * PAGE_SIZE, PAGE_SIZE);
            mmap(USER_START + i * PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd,
                 vm->map[i] * PAGE_SIZE);
        }
    }
}

static void doswitch(void) {
    struct task *old = current;
    current = runq;
    runq = current->next;

    current_start = sched_gettime();
    vmctx_apply(&current->vm);
    ctx_switch(&old->ctx, &current->ctx);
}

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

void sched_new(void (*entrypoint)(void *aspace),
               void *aspace,
               int priority) {

    struct task *t = pool_alloc(&taskpool);
    t->entry = entrypoint;
    t->as = aspace;
    t->priority = priority;
    t->next = NULL;

    vmctx_make(&t->vm, 8192);
    ctx_make(&t->ctx, tasktramp0, t->stack, sizeof(t->stack));

    if (!lastpending) {
        lastpending = t;
        pendingq = t;
    } else {
        lastpending->next = t;
        lastpending = t;
    }
}

void sched_sleep(unsigned ms) {

    if (!ms) {
        irq_disable();
        policy_run(current);
        doswitch();
        irq_enable();
        return;
    }

    current->waketime = sched_gettime() + ms;

    int curtime;
    while ((curtime = sched_gettime()) < current->waketime) {
        irq_disable();
        struct task **c = &waitq;
        while (*c && (*c)->waketime < current->waketime) {
            c = &(*c)->next;
        }
        current->next = *c;
        *c = current;

        doswitch();
        irq_enable();
    }
}

static int fifo_cmp(struct task *t1, struct task *t2) {
    return -1;
}

static int prio_cmp(struct task *t1, struct task *t2) {
    return t2->priority - t1->priority;
}

static void hctx_push(greg_t *regs, unsigned long val) {
    regs[REG_RSP] -= sizeof(unsigned long);
    *(unsigned long *) regs[REG_RSP] = val;
}

static void bottom(void) {
    time += TICK_PERIOD;

    while (waitq && waitq->waketime <= sched_gettime()) {
        struct task *t = waitq;
        waitq = waitq->next;
        policy_run(t);
    }

    if (TICK_PERIOD <= sched_gettime() - current_start) {
        irq_disable();
        policy_run(current);
        doswitch();
        irq_enable();
    }
}

static void top(int sig, siginfo_t *info, void *ctx) {
    ucontext_t *uc = (ucontext_t *) ctx;
    greg_t *regs = uc->uc_mcontext.gregs;

    unsigned long oldsp = regs[REG_RSP];
    regs[REG_RSP] -= SYSV_REDST_SZ;
    hctx_push(regs, regs[REG_RIP]);
    hctx_push(regs, oldsp);
    hctx_push(regs, (unsigned long) (current->stack + sizeof(current->stack) - 16));
    hctx_push(regs, (unsigned long) bottom);
    regs[REG_RIP] = (greg_t) tramptramp;
}

long sched_gettime(void) {
    int cnt1 = timer_cnt() / 1000;
    int time1 = time;
    int cnt2 = timer_cnt() / 1000;
    int time2 = time;

    return (cnt1 <= cnt2) ?
           time1 + cnt2 :
           time2 + cnt2;
}

void sched_run(enum policy policy) {
    int (*policies[])(struct task *t1, struct task *t2) = {fifo_cmp, prio_cmp};
    policy_cmp = policies[policy];

    struct task *t = pendingq;
    while (t) {
        struct task *next = t->next;
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

    while (runq || waitq) {
        if (runq) {
            policy_run(current);
            doswitch();
        } else {
            sigsuspend(&none);
        }

    }

    irq_enable();
}

static void sighnd(int sig, siginfo_t *info, void *ctx) {
    ucontext_t *uc = (ucontext_t *) ctx;
    greg_t *regs = uc->uc_mcontext.gregs;

    uint16_t insn = *(uint16_t *) regs[REG_RIP];
    if (insn != 0x81cd) {
        abort();
    }

    regs[REG_RAX] = syscall_do(regs[REG_RAX], regs[REG_RBX],
                               regs[REG_RCX], regs[REG_RDX],
                               regs[REG_RSI], (void *) regs[REG_RDI]);

    regs[REG_RIP] += 2;
}

int main(int argc, char *argv[]) {
    struct sigaction act = {
            .sa_sigaction = sighnd,
            .sa_flags = SA_RESTART,
    };
    sigemptyset(&act.sa_mask);

    if (-1 == sigaction(SIGSEGV, &act, NULL)) {
        perror("signal set failed");
        return 1;
    }

    memfd = memfd_create("mem", 0);
    if (memfd < 0) {
        perror("memfd_create");
        return 1;
    }

    if (ftruncate(memfd, PAGE_SIZE * MEM_PAGES) < 0) {
        perror("ftrucate");
        return 1;
    }

    shell(0, NULL);
}
