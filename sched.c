#define _GNU_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"

struct task {
    void (*entry)(void *as);

    void *as;
    int priority;
    int deadline;

    // timeout support
    int waketime;

    // policy support
    struct task *next;
};

static int time;

static struct task *current;
static struct task *runq;
static struct task *waitq;

static struct task *pendingq;
static struct task *lastpending;

static int (*policy_cmp)(struct task *t1, struct task *t2);

static struct task taskarray[16];
static struct pool taskpool = POOL_INITIALIZER_ARRAY(taskarray);

static sigset_t set;

void irq_disable(void) {
    sigprocmask(SIG_BLOCK, &set, NULL);
    sigaddset(&set, SIGALRM);
}

void irq_enable(void) {
    sigprocmask(SIG_UNBLOCK, &set, NULL);
    sigaddset(&set, SIGALRM);
}

static void policy_run(struct task *t) {
    struct task **c = &runq;

    while (*c && policy_cmp(*c, t) <= 0) {
        c = &(*c)->next;
    }
    t->next = *c;
    *c = t;
}

void sched_new(void (*entrypoint)(void *aspace),
               void *aspace,
               int priority,
               int deadline) {

    struct task *t = pool_alloc(&taskpool);;
    t->entry = entrypoint;
    t->as = aspace;
    t->priority = priority;
    t->deadline = 0 < deadline ? deadline : INT_MAX;
    t->next = NULL;

    if (!lastpending) {
        lastpending = t;
        pendingq = t;
    } else {
        lastpending->next = t;
        lastpending = t;
    }
}

void sched_cont(void (*entrypoint)(void *aspace),
                void *aspace,
                int timeout) {

    if (current->next != current) {
        fprintf(stderr, "Mulitiple sched_cont\n");
        return;
    }

    irq_disable();

    if (!timeout) {
        policy_run(current);
        goto out;
    }

    current->waketime = time + timeout;

    struct task **c = &waitq;
    while (*c && (*c)->waketime < current->waketime) {
        c = &(*c)->next;
    }
    current->next = *c;
    *c = current;

    out:
    irq_enable();
}

void sched_time_elapsed(unsigned amount) {
    irq_disable();

    int endtime = time + amount;
    while (time < endtime) {
        irq_enable();
        pause();
        irq_disable();
    }

    irq_enable();
}

static int fifo_cmp(struct task *t1, struct task *t2) {
    return -1;
}

static int prio_cmp(struct task *t1, struct task *t2) {
    return t2->priority - t1->priority;
}

static int deadline_cmp(struct task *t1, struct task *t2) {
    int d = t1->deadline - t2->deadline;
    if (d) {
        return d;
    }
    return prio_cmp(t1, t2);
}

static void tick_hnd(void) {
    time++;

    while (waitq != NULL) {
        if (waitq->waketime <= sched_gettime()) {
            struct task *t = waitq;
            waitq = waitq->next;
            policy_run(t);
        } else {
            break;
        }
    }
}

long sched_gettime(void) {
    int cur_time = time;

    int cnt = timer_cnt();

    if (timer_cnt() >= cnt) {
        return cur_time + cnt / 1000;
    } else {
        return cur_time + cnt / 1000;
    }
}

void sched_run(enum policy policy) {
    int (*policies[])(struct task *t1, struct task *t2) = {fifo_cmp, prio_cmp, deadline_cmp};
    policy_cmp = policies[policy];

    struct task *t = pendingq;
    while (t) {
        struct task *next = t->next;
        policy_run(t);
        t = next;
    }

    timer_init(1, tick_hnd);

    irq_disable();

    while (runq) {
        current = runq;
        if (current) {
            runq = current->next;
            current->next = current;
        }

        irq_enable();
        if (current) {
            current->entry(current->as);
        } else {
            pause();
        }
        irq_disable();
    }

    irq_enable();
}
