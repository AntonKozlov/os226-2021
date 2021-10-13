#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "sched.h"

static unsigned time;

struct sched_task *curr_task = NULL;

struct sched_task {
    void (*entrypoint)(void *aspace);

    void *aspace;
    int priority;
    int deadline;
    unsigned time_ready;
};

typedef int (*sched_cmp)(struct sched_task, struct sched_task);

struct sched_list_node {
    struct sched_task task;
    struct sched_list_node *next;
};

static struct sched_task_list {
    struct sched_list_node *head;
} sched_task_list = {.head = NULL};

int sched_add_task(struct sched_task task) {
    struct sched_list_node *node = malloc(sizeof(struct sched_list_node));

    if (node == NULL) {
        fprintf(stderr, "ERROR: Can not allocate memory for task!\n");
        return 0;
    }

    node->task = task;
    node->next = sched_task_list.head;
    sched_task_list.head = node;
    return 1;
}

void sched_run_task(struct sched_list_node *list_node) {
    curr_task = &(list_node->task);
    curr_task->entrypoint(curr_task->aspace);
    curr_task = NULL;
    free(list_node);
}

void sched_run_tasks(sched_cmp cmp) {

    while (sched_task_list.head) {

        struct sched_list_node *prev_list_node = NULL;
        struct sched_list_node *list_node = NULL;
        struct sched_list_node *curr_prev = NULL;

        for (struct sched_list_node *curr_list_node = sched_task_list.head;
             curr_list_node;
             curr_list_node = curr_list_node->next) {

            if ((!list_node && curr_list_node->task.time_ready <= time) ||
                (list_node && cmp(list_node->task, curr_list_node->task))) {

                prev_list_node = curr_prev;
                list_node = curr_list_node;

            }
            curr_prev = curr_list_node;
        }
        if (list_node) {
            if (prev_list_node) {
                prev_list_node->next = list_node->next;
            } else {
                sched_task_list.head = list_node->next;
            }
            sched_run_task(list_node);
        }
    }
}

int sched_cmp_prio(struct sched_task old_task,
                   struct sched_task new_task) {
    return new_task.time_ready <= time &&
           old_task.priority <= new_task.priority;
}

int sched_cmp_fifo(struct sched_task old_task,
                   struct sched_task new_task) {
    return new_task.time_ready <= time;
}

int sched_cmp_deadline(struct sched_task old_task,
                       struct sched_task new_task) {

    if (new_task.time_ready > time) {
        return 0;
    }
    return ((new_task.deadline > 0 && (new_task.deadline < old_task.deadline || old_task.deadline <= 0))
            || (new_task.deadline == old_task.deadline && new_task.priority >= old_task.priority));

}

void sched_new(void (*entrypoint)(void *aspace),
               void *aspace,
               int priority,
               int deadline) {

    struct sched_task task = {entrypoint,
                              aspace,
                              priority,
                              deadline,
                              time};

    if (!sched_add_task(task)) {
        fprintf(stderr, "ERROR: Task can't be added to the scheduler!\n");
    }
}

void sched_cont(void (*entrypoint)(void *aspace),
                void *aspace,
                int timeout) {
    if (curr_task == NULL) {
        fprintf(stderr, "ERROR: No task is currently running!\n");
        return;
    }
    struct sched_task task = {entrypoint,
                              aspace,
                              curr_task->priority,
                              curr_task->deadline,
                              time + timeout};
    sched_add_task(task);
}

void sched_time_elapsed(unsigned amount) {
    time += amount;
}

void sched_run(enum policy policy) {
    switch (policy) {
        case POLICY_PRIO:
            sched_run_tasks(&sched_cmp_prio);
            break;
        case POLICY_FIFO:
            sched_run_tasks(&sched_cmp_prio);
            break;
        case POLICY_DEADLINE:
            sched_run_tasks(&sched_cmp_deadline);
            break;
        default:
            fprintf(stderr, "ERROR: Invalid policy!\n");
            break;
    }
}