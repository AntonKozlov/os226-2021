#define _GNU_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"
#include "ctx.h"

/* AMD64 Sys V ABI, 3.2.2 The Stack Frame:
The 128-byte area beyond the location pointed to by %rsp is considered to
be reserved and shall not be modified by signal or interrupt handlers */
#define SYSV_REDST_SZ 128

#define TICK_PERIOD 100

extern void tramptramp(void);

struct task {
    char stack[8192];
    struct ctx ctx;

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

void irq_disable(void) {
    sigprocmask(SIG_BLOCK, &irqs, NULL);
}

void irq_enable(void) {
    sigprocmask(SIG_UNBLOCK, &irqs, NULL);
}

static void policy_run(struct task *t) {
    struct task **c = &runq;

    while (*c && (t == idle || policy_cmp(*c, t) <= 0)) {
        c = &(*c)->next;
    }
    t->next = *c;
    *c = t;
}

static void doswitch(void) {
    struct task *old = current;
    current = runq;
    runq = current->next;

    current_start = sched_gettime();
    ctx_switch(&old->ctx, &current->ctx);
}

static void tasktramp(void) {
    irq_enable();
    current->entry(current->as);
    irq_disable();
}

void sched_new(void (*entrypoint)(void *aspace),
               void *aspace,
               int priority) {

    struct task *t = pool_alloc(&taskpool);
    t->entry = entrypoint;
    t->as = aspace;
    t->priority = priority;
    t->next = NULL;

    ctx_make(&t->ctx, tasktramp, t->stack, sizeof(t->stack));

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

    irq_disable();
    policy_run(current);
    while (waitq != NULL && waitq->waketime <= sched_gettime()) {
        struct task *t = waitq;
        waitq = waitq->next;
        policy_run(t);
    }
    doswitch();
    irq_enable();
}

static void top(int sig, siginfo_t *info, void *ctx) {
    ucontext_t *uc = (ucontext_t *) ctx;
    greg_t *regs = uc->uc_mcontext.gregs;

    unsigned long oldsp = regs[REG_RSP];
    regs[REG_RSP] -= SYSV_REDST_SZ;
    hctx_push(regs, regs[REG_RIP]);
    hctx_push(regs, sig);
    hctx_push(regs, regs[REG_RBP]);
    hctx_push(regs, oldsp);
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

