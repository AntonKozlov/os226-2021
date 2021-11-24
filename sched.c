#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"
#include "ctx.h"
#include "syscall.h"
#include "usyscall.h"

/* AMD64 Sys V ABI, 3.2.2 The Stack Frame:
   The 128-byte area beyond the location pointed to by %rsp is considered to
   be reserved and shall not be modified by signal or interrupt handlers */
#define SYSV_REDST_SZ 128

#define TICK_PERIOD 100

#define MEM_PAGES 1024 // The number of pages in the physical memory
#define PAGE_SIZE 4096 // Page size in bytes

#define USER_PAGES 1024 // The number of pages in the virtual memory
#define USER_START ((void*)IUSERSPACE_START) // Starting address of the virtual memory
#define USER_STACK_PAGES 2 // The number of pages occupied by the stack in the virtual memory

extern int shell(int argc, char* argv[]);

extern void tramptramp(void);

extern void exittramp(void);

struct vmctx {
    unsigned int map[USER_PAGES];
    unsigned int brk;
    unsigned int stack;
};

struct task {
    char stack[8192];
    struct vmctx vm;

    union {
        struct ctx ctx;

        struct {
            int (* main)(int, char**);

            int argc;
            char** argv;
        };
    };

    void (* entry)(void* as);

    void* as;
    int priority;

    // timeout support
    unsigned int waketime;

    // policy support
    struct task* next;
};

struct savedctx {
    unsigned long rbp;
    unsigned long r15;
    unsigned long r14;
    unsigned long r13;
    unsigned long r12;
    unsigned long r11;
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long rdi;
    unsigned long rsi;
    unsigned long rdx;
    unsigned long rcx;
    unsigned long rbx;
    unsigned long rax;
    unsigned long rflags;
    unsigned long bottom;
    unsigned long stack;
    unsigned long sig;
    unsigned long oldsp;
    unsigned long rip;
};

static void syscallbottom(unsigned long sp);

static int do_fork(unsigned long sp);

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
#define LONG_BITS (sizeof(unsigned long) * CHAR_BIT)
static unsigned long bitmap_pages[
        MEM_PAGES / LONG_BITS]; // Bitmap representing the allocation status of the memory pages

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

    if (bitmap_elem == NULL) {
        fprintf(stderr, "cannot find free page\n");
        abort();
    }

    int bit_num = ffsl((long) *bitmap_elem + 1) - 1;
    *bitmap_elem |= 1 << bit_num;

    return (int) ((bitmap_elem - bitmap) * LONG_BITS) + bit_num;
}

static void bitmap_free(unsigned long* bitmap, size_t size, unsigned v) {
    bitmap[v / LONG_BITS] &= ~(1 << (v % LONG_BITS));
}

// Adds the task to the run queue
static void policy_run(struct task* t) {
    struct task** c = &runq;

    while (*c != NULL && (t == idle || policy_cmp(*c, t) <= 0)) c = &((*c)->next);
    t->next = *c;
    *c = t;
}

static void vmctx_make(struct vmctx* vm, size_t stack_size) {
    vm->stack = USER_PAGES - stack_size / PAGE_SIZE;
    memset(vm->map, -1, sizeof(vm->map));
    for (int i = 0; i < stack_size / PAGE_SIZE; i++) {
        int mempage = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
        vm->map[USER_PAGES - 1 - i] = mempage;
    }
}

// Applies the virtual memory mapping (switches the context)
static void vmctx_apply(struct vmctx* vm) {
    if (munmap(USER_START, USER_PAGES * PAGE_SIZE) == -1) {
        perror("munmap");
        abort();
    }

    for (int i = 0; i < USER_PAGES; i++) {
        if (vm->map[i] == -1) continue;

        if (mmap(USER_START + i * PAGE_SIZE, PAGE_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED_NOREPLACE, memfd, vm->map[i] * PAGE_SIZE) == MAP_FAILED) {
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

// Creates context for the task and adds it to the pending queue
struct task* sched_new(void (* entrypoint)(void* aspace), void* aspace, int priority) {
    struct task* t = pool_alloc(&taskpool);
    t->entry = entrypoint;
    t->as = aspace;
    t->priority = priority;
    t->next = NULL;

    ctx_make(&t->ctx, tasktramp, t->stack + sizeof(t->stack));

    return t;
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

static void timerbottom() {
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

static unsigned long bottom(unsigned long sp, int sig) {
    if (sig == SIGALRM) timerbottom();
    else if (sig == SIGSEGV) syscallbottom(sp);
    return sp;
}

static void top(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* uc = (ucontext_t*) ctx;
    greg_t* regs = uc->uc_mcontext.gregs;

    unsigned long oldsp = regs[REG_RSP];
    regs[REG_RSP] -= SYSV_REDST_SZ;
    hctx_push(regs, regs[REG_RIP]);
    hctx_push(regs, oldsp);
    hctx_push(regs, sig);
    hctx_push(regs, (unsigned long) (current->stack + sizeof(current->stack) - 16));
    hctx_push(regs, (unsigned long) bottom);
    regs[REG_RIP] = (greg_t) tramptramp;
}

long sched_gettime(void) {
    // TODO: replace when timer gets fixed (i.e. initialization is added)
    // return time + timer_cnt() / 1000;
    return time;
}

void sched_run(void) {
    sigemptyset(&irqs);
    sigaddset(&irqs, SIGALRM);

    /*timer_init(TICK_PERIOD, top);*/

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

static void syscallbottom(unsigned long sp) {
    struct savedctx* sc = (struct savedctx*) sp;

    uint16_t insn = *(uint16_t*) sc->rip;
    if (insn != 0x81cd) abort();

    sc->rip += 2;

    if (sc->rax == os_syscall_nr_fork) sc->rax = do_fork(sp);
    else {
        sc->rax = syscall_do((int) sc->rax, sc->rbx,
                             sc->rcx, sc->rdx,
                             sc->rsi, (void*) sc->rdi);
    }
}

static int vmctx_brk(struct vmctx* vm, void* addr) {
    long newbrk = (addr - USER_START + PAGE_SIZE - 1) / PAGE_SIZE;
    if (newbrk < 0 || USER_PAGES <= newbrk) {
        fprintf(stderr, "Out-of-mem\n");
        abort();
    }

    for (unsigned int i = vm->brk; i < newbrk; i++) {
        vm->map[i] = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
    }
    for (unsigned int i = newbrk; i < vm->brk; i++) {
        bitmap_free(bitmap_pages, sizeof(bitmap_pages), vm->map[i]);
    }
    vm->brk = newbrk;

    return 0;
}

static int vmprotect(void* start, unsigned len, int prot) {
    if (mprotect(start, len, prot) == -1) {
        perror("mprotect");
        return -1;
    }
    return 0;
}

static void exectramp(void) {
    irq_enable();
    current->main(current->argc, current->argv);
    irq_disable();
    abort();
}

static int do_exec(const char* path, char* argv[]) {
    // TODO: remove the hardcoded string when snprintf gets fixed
    char elfpath[32] = "init.app";
    // snprintf(elfpath, sizeof(elfpath), "%s.app", path);
    int fd = open(elfpath, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    void* rawelf = mmap(NULL, 128 * 1024, PROT_READ, MAP_PRIVATE, fd, 0);
    if (rawelf == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    if (strncmp(rawelf, "\x7f" "ELF" "\x2", 5) != 0) {
        printf("ELF header mismatch\n");
        return 1;
    }

    const Elf64_Ehdr* ehdr = (Elf64_Ehdr*) rawelf;

    if (ehdr->e_type != ET_EXEC || ehdr->e_phoff == 0 || ehdr->e_phnum == 0 ||
        ehdr->e_phentsize != sizeof(Elf64_Phdr) || ehdr->e_entry == 0) {
        fprintf(stderr, "invalid ELF header\n");
        return 1;
    }

    const Elf64_Phdr* phdrs_start = (Elf64_Phdr*) (rawelf + ehdr->e_phoff);

    void* max_addr = USER_START;
    for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = (Elf64_Phdr*) phdrs_start + i;
        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_vaddr < IUSERSPACE_START) fprintf(stderr, "bad section\n");
        if ((void*) (phdr->p_vaddr + phdr->p_memsz) > max_addr) max_addr = (void*) (phdr->p_vaddr + phdr->p_memsz);
    }

    char** copyargv = USER_START + (USER_PAGES - 1) * PAGE_SIZE;
    char* copybuf = (char*) (copyargv + 32);
    char* const* arg = argv;
    char** copyarg = copyargv;
    while (*arg != NULL) {
        *copyarg++ = strcpy(copybuf, *arg++);
        copybuf += strlen(copybuf) + 1;
    }
    *copyarg = NULL;

    if (vmctx_brk(&current->vm, max_addr) != 0) {
        fprintf(stderr, "vmctx_brk failed\n");
        return 1;
    }
    vmctx_apply(&current->vm);
    if (vmprotect(USER_START, max_addr - USER_START, PROT_READ | PROT_WRITE) != 0) {
        fprintf(stderr, "vmprotect read-write failed\n");
        return 1;
    }

    for (unsigned int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = (Elf64_Phdr*) phdrs_start + i;
        if (phdr->p_type != PT_LOAD) continue;

        memcpy((void*) phdr->p_vaddr, rawelf + phdr->p_offset, phdr->p_filesz);

        int prot = ((phdr->p_flags & PF_X) ? PROT_EXEC : 0) |
                   ((phdr->p_flags & PF_R) ? PROT_READ : 0) |
                   ((phdr->p_flags & PF_W) ? PROT_WRITE : 0);
        if (vmprotect((void*) phdr->p_vaddr, phdr->p_memsz, prot) != 0) {
            fprintf(stderr, "vmprotect section failed\n");
            return 1;
        }
    }

    struct ctx dummy, new;
    ctx_make(&new, exectramp, (char*) copyargv);

    current->main = (int (*)(int, char**)) ehdr->e_entry;
    current->argv = copyargv;
    current->argc = (int) (copyarg - copyargv);

    ctx_switch(&dummy, &new);

    if (munmap(rawelf, 128 * 1024) == -1) {
        perror("munmap");
        abort();
    }

    return 0;
}

static void inittramp(void* arg) {
    char* args = {NULL};
    do_exec("init", &args);
}

static void forktramp(void* arg) {
    vmctx_apply(&current->vm);

    struct ctx dummy, new;
    ctx_make(&new, exittramp, arg);
    ctx_switch(&dummy, &new);
}

static void copyrange(struct vmctx* vm, unsigned from, unsigned to) {
    for (unsigned i = from; i < to; ++i) {
        vm->map[i] = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
        if (vm->map[i] == -1) abort();
        if (pwrite(memfd,
                   USER_START + i * PAGE_SIZE,
                   PAGE_SIZE,
                   vm->map[i] * PAGE_SIZE) == -1) {
            perror("pwrite");
            abort();
        }
    }
}

static void vmctx_copy(struct vmctx* dst, struct vmctx* src) {
    dst->brk = src->brk;
    dst->stack = src->stack;
    copyrange(dst, 0, src->brk);
    copyrange(dst, src->stack, USER_PAGES - 1);
}

static int do_fork(unsigned long sp) {
    struct task* t = sched_new(forktramp, (void*) sp, 0);
    vmctx_copy(&t->vm, &current->vm);
    policy_run(t);
}

int sys_exit(int code) {
    doswitch();
}

int main(int argc, char* argv[]) {
    struct sigaction act = {
            .sa_sigaction = top,
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

    policy_cmp = prio_cmp;
    struct task* t = sched_new(inittramp, NULL, 0);
    vmctx_make(&t->vm, 4 * PAGE_SIZE);
    policy_run(t);
    sched_run();
}
