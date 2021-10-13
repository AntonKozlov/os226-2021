#include <stdio.h>
#include <malloc.h>

#include "sched.h"

#define ADD_ERR "ERROR: Task can't be added to the scheduler!\n"
#define INVALID_POLICY_ERR "ERROR: Invalid policy!\n"

static unsigned time;

struct sched_task* curr_task = NULL;

struct sched_task {
    void (* entrypoint)(void* aspace);
    void* aspace;
    int priority;
    int deadline;
    unsigned time_ready;
};

struct sched_list_node {
    struct sched_task task;
    struct sched_list_node* next;
};

static struct sched_task_list {
    struct sched_list_node* first;
} sched_task_list = {NULL};

int add_task(struct sched_task task) {
    struct sched_list_node* node = malloc(sizeof(struct sched_list_node));
    if (node == NULL) {
        return 0;
    }
    node->task = task;
    node->next = sched_task_list.first;
    sched_task_list.first = node;
    return 1;
}

void run_task(struct sched_list_node* list_node) {
    curr_task = &(list_node->task);
    curr_task->entrypoint(curr_task->aspace);
    curr_task = NULL;
    free(list_node);
}

void run_tasks(int (* check)(struct sched_task old_task, struct sched_task new_task)) {
    while (sched_task_list.first != NULL) {
        struct sched_list_node* prev_list_node = NULL, * list_node = NULL;
        struct sched_list_node* curr_prev = NULL;
        for (struct sched_list_node* curr_list_node = sched_task_list.first;
             curr_list_node != NULL;
             curr_list_node = curr_list_node->next) {
            if ((list_node == NULL && curr_list_node->task.time_ready <= time) ||
                (list_node != NULL && check(list_node->task, curr_list_node->task))) {
                prev_list_node = curr_prev;
                list_node = curr_list_node;
            }
            curr_prev = curr_list_node;
        }
        if (list_node != NULL) {
            if (prev_list_node != NULL) prev_list_node->next = list_node->next;
            else sched_task_list.first = list_node->next;
            run_task(list_node);
        }
    }
}

int check_priority(struct sched_task old_task, struct sched_task new_task) {
    return new_task.time_ready <= time && new_task.priority >= old_task.priority;
}

int check_fifo(struct sched_task old_task, struct sched_task new_task) {
    return new_task.time_ready <= time;
}

int check_deadline(struct sched_task old_task, struct sched_task new_task) {
    return new_task.time_ready <= time &&
           ((0 < new_task.deadline && (new_task.deadline < old_task.deadline || old_task.deadline <= 0))
            || (new_task.deadline == old_task.deadline && new_task.priority >= old_task.priority));
}

void sched_new(void (* entrypoint)(void* aspace), void* aspace, int priority, int deadline) {
    struct sched_task task = {entrypoint, aspace, priority, deadline, time};
    if (!add_task(task)) {
        fprintf(stderr, ADD_ERR);
    }
}

void sched_cont(void (* entrypoint)(void* aspace), void* aspace, int timeout) {
    if (curr_task == NULL) return;
    struct sched_task task = {entrypoint, aspace, curr_task->priority, curr_task->deadline, time + timeout};
    add_task(task);
}

void sched_time_elapsed(unsigned amount) {
    time += amount;
}

void sched_run(enum policy policy) {
    switch (policy) {
        case POLICY_PRIO:
            run_tasks(&check_priority);
            break;
        case POLICY_FIFO:
            run_tasks(&check_fifo);
            break;
        case POLICY_DEADLINE:
            run_tasks(&check_deadline);
            break;
        default:
            fprintf(stderr, INVALID_POLICY_ERR);
    }
}
