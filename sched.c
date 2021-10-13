#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "sched.h"

static int time = 0;

typedef int (policy_cmp)(struct sched_task *, struct sched_task *);

struct sched_task {
    void (*entrypoint)(void *aspace);

    void *aspace;
    int priority;
    int deadline;
    int start;
};

struct sched_node {
    struct sched_node *next;
    struct sched_task task;
};

struct sched_node *sched_head = NULL;

struct sched_task *curr_task = NULL;

static int round_robin_cmp(struct sched_task *first_task, struct sched_task *second_task) {
    return second_task->start <= time;
}

static int priority_cmp(struct sched_task *first_task, struct sched_task *second_task) {
    return second_task->start <= time && second_task->priority >= first_task->priority;
}

static int deadline_cmp(struct sched_task *first_task, struct sched_task *second_task) {
    return second_task->start <= time &&
            ((0 < second_task->deadline && (second_task->deadline < first_task->deadline || first_task->deadline <= 0))
            || (second_task->deadline == first_task->deadline));
}

struct sched_node *get_from_sched(struct sched_node *schedule, struct sched_node *task) {
    return schedule->next;
}

void add_new_task(struct sched_task task) {
    struct sched_node *node = malloc(sizeof(struct sched_node));
    node->task = task;
    node->next = sched_head;
    sched_head = node;
}

void sched_new(void (*entrypoint)(void *aspace),
               void *aspace,
               int priority,
               int deadline) {
    struct sched_task task = {
            entrypoint,
            aspace,
            priority,
            deadline,
            0
    };
    add_new_task(task);
}

void sched_cont(void (*entrypoint)(void *aspace),
                void *aspace,
                int timeout) {
    if (curr_task == NULL) {
        fprintf(stderr, "Error with sched_cont: there is no running task");
        return;
    }
    struct sched_task task = {
            entrypoint,
            aspace,
            curr_task->priority,
            curr_task->deadline,
            time + timeout
    };
    add_new_task(task);
}

void sched_time_elapsed(unsigned amount) { time += amount; }

void exec_tasks(policy_cmp cmp) {
    while (sched_head != NULL) {
        struct sched_node *previous = NULL, *node = NULL;
        for (struct sched_node *i = NULL, *cur_node = sched_head; cur_node != NULL; cur_node = cur_node->next) {
            if ((node == NULL && cur_node->task.start <= time) || (node != NULL && cmp(&node->task, &cur_node->task))) {
                previous = i;
                node = cur_node;
            }
            i = cur_node;
        }
        if (node != NULL) {
            if (previous != NULL) previous->next = node->next;
            else sched_head = node->next;

            curr_task = &node->task;
            curr_task->entrypoint(curr_task->aspace);
            curr_task = NULL;
            free(node);
        }
    }
}

void sched_run(enum policy policy) {
    switch (policy) {
        case POLICY_FIFO:
            exec_tasks(round_robin_cmp);
            break;
        case POLICY_PRIO:
            exec_tasks(priority_cmp);
            break;
        case POLICY_DEADLINE:
            exec_tasks(deadline_cmp);
            break;
        default:
            fprintf(stderr, "Unknown policy");
    }
}
