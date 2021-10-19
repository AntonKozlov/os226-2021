#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"

#define TIME_PERIOD_MS 1

static int time = 0;

void irq_disable(void) {
    sigset_t sigset;
    if (sigemptyset(&sigset) || sigaddset(&sigset, SIGALRM) || sigprocmask(SIG_BLOCK, &sigset, NULL))
        fprintf(stderr, "irq_disable() error");
}

void irq_enable(void) {
    sigset_t sigset;
    if (sigemptyset(&sigset) || sigaddset(&sigset, SIGALRM) || sigprocmask(SIG_UNBLOCK, &sigset, NULL))
        fprintf(stderr, "irq_enable() error");
}

static void tick_hnd(void) {
    time += TIME_PERIOD_MS;
}

long sched_gettime(void) {
    return time + timer_cnt() / 1000;
}

struct task {
    void (*entrypoint)(void *);

    void *aspace;
    int priority;
    int deadline;
    int waketime;
    struct task *next;
} task_array[16];

struct task_list {
    struct task *first;
} task_list = {NULL};

#define POLICY_X(X) \
        X(policy_fifo) \
        X(policy_prio) \
        X(policy_deadline)

#define DECLARE(X) extern struct task *X(struct task *chosen_task, struct task *next_task);

POLICY_X(DECLARE)

#undef DECLARE
#undef POLICY_X

void run_tasks(struct task *(*)(struct task *, struct task *));

struct task *current = NULL;

struct pool task_pool = POOL_INITIALIZER_ARRAY(task_array)

void add_task(void (*entrypoint)(void *),
              void *aspace,
              int priority,
              int deadline,
              int timeout
) {
    struct task *task = pool_alloc(&task_pool);
    task->entrypoint = entrypoint;
    task->aspace = aspace;
    task->priority = priority;
    task->deadline = deadline;
    task->waketime = timeout;
    task->next = task_list.first;
    task_list.first = task;
}

void delete_task() {
    if (current == NULL) return;
    if (current == task_list.first) {
        task_list.first = current->next;
    } else {
        struct task *prev;
        for (prev = task_list.first; prev->next != current; prev = prev->next);
        prev->next = current->next;
    }
}

void sched_new(void (*entrypoint)(void *),
               void *aspace,
               int priority,
               int deadline
) {
    add_task(entrypoint, aspace, priority, deadline, time);
}

void sched_cont(void (*entrypoint)(void *),
                void *aspace,
                int timeout) {
    irq_disable();
    if (current != NULL) add_task(entrypoint, aspace, current->priority, current->deadline, time + timeout);
    irq_enable();
}

void sched_run(enum policy policy) {
    timer_init(TIME_PERIOD_MS, tick_hnd);
    irq_disable();
    if (policy == POLICY_FIFO) run_tasks(policy_fifo);
    else if (policy == POLICY_PRIO) run_tasks(policy_prio);
    else if (policy == POLICY_DEADLINE) run_tasks(policy_deadline);
    irq_enable();
}

void sched_time_elapsed(unsigned amount) {
    irq_disable();
    int endtime = time + (int) amount;
    while (time < endtime) {
        irq_enable();
        pause();
        irq_disable();
    }
    irq_enable();
}

void run_tasks(struct task *(*policy)(struct task *, struct task *)) {
    while (task_list.first != NULL) {
        struct task *chosen_task = task_list.first, *next_task = chosen_task->next;
        for (; next_task != NULL; next_task = next_task->next)
            chosen_task = policy(chosen_task, next_task);
        if (chosen_task->waketime > time) {
            irq_enable();
            pause();
            irq_disable();
            continue;
        }
        current = chosen_task;

        irq_enable();
        current->entrypoint(current->aspace);
        irq_disable();

        delete_task();

        pool_free(&task_pool, current);
        current = NULL;
    }
}

struct task *policy_fifo(struct task *chosen_task, struct task *next_task) {
    return (next_task->waketime > time) ? chosen_task : next_task;
}

struct task *policy_prio(struct task *chosen_task, struct task *next_task) {
    if (next_task->waketime > time) return chosen_task;
    return (chosen_task->waketime <= time && chosen_task->priority > next_task->priority) ?
           chosen_task : next_task;
}

struct task *policy_deadline(struct task *chosen_task, struct task *next_task) {
    return next_task->waketime <= time &&
           ((0 < next_task->deadline && (next_task->deadline < chosen_task->deadline || chosen_task->deadline <= 0)) ||
            (next_task->deadline == chosen_task->deadline && next_task->priority >= chosen_task->priority)) ? next_task
                                                                                                            : chosen_task;
}
