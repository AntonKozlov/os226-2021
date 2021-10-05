#include <string.h>
#include <stdio.h>

#include "sched.h"
#include "pool.h"

static int time = 0;

struct sched_task {
    void (*entrypoint)(void *aspace);

    void *aspace;
    int priority;
    int deadline; // must be non-negative
    int min_start_time;
};

static struct sched_task *sched_cur_task = NULL;

typedef int (*sched_task_comparator)(struct sched_task *, struct sched_task *);

static struct sched_node {
    struct sched_node *next;
    struct sched_task task;
} sched_node_arr[16];

static struct pool sched_node_pool = POOL_INITIALIZER_ARRAY(sched_node_arr)
static struct sched_node *sched_list_head = NULL;

static void sched_schedule(struct sched_task task) {
    struct sched_node *node = pool_alloc(&sched_node_pool);
    if (node == NULL) {
        fprintf(stderr, "Unable to allocate memory for scheduler task node\n");
        return;
    }
    node->next = sched_list_head;
    node->task = task;
    sched_list_head = node;
}

void sched_new(void (*entrypoint)(void *aspace),
               void *aspace,
               int priority,
               int deadline) {
    sched_schedule((struct sched_task) {
            .entrypoint = entrypoint,
            .aspace = aspace,
            .priority = priority,
            .deadline = deadline > 0 ? deadline : 0,
            .min_start_time = 0,
    });
}

void sched_cont(void (*entrypoint)(void *aspace),
                void *aspace,
                int timeout) {
    if (sched_cur_task == NULL) {
        fprintf(stderr, "Unable to schedule continuation as no task is currently running\n");
        return;
    }
    sched_schedule((struct sched_task) {
            .entrypoint = entrypoint,
            .aspace = aspace,
            .priority = sched_cur_task->priority,
            .deadline = sched_cur_task->deadline,
            .min_start_time = time + timeout,
    });
}

static void sched_do_run(sched_task_comparator comparator) {
    while (sched_list_head != NULL) {
        struct sched_node **max_node_ptr = NULL;
        for (struct sched_node **node_ptr = &sched_list_head; *node_ptr != NULL; node_ptr = &((*node_ptr)->next)) {
            if ((*node_ptr)->task.min_start_time <= time &&
                (max_node_ptr == NULL || comparator(&(*node_ptr)->task, &(*max_node_ptr)->task) >= 0))
                max_node_ptr = node_ptr;
        }
        if (max_node_ptr != NULL) {
            struct sched_node *max_node = *max_node_ptr;
            *max_node_ptr = max_node->next;
            sched_cur_task = &(max_node->task);
            sched_cur_task->entrypoint(sched_cur_task->aspace);
            sched_cur_task = NULL;
            pool_free(&sched_node_pool, max_node);
        }
    }
}

static int sched_compare_task_by_fifo(struct sched_task *task1, struct sched_task *task2) {
    return 0;
}

static int sched_compare_task_by_prio(struct sched_task *task1, struct sched_task *task2) {
    if (task1->priority == task2->priority) return 0;
    return task1->priority > task2->priority ? 1 : -1;
}

static int sched_compare_task_by_deadline(struct sched_task *task1, struct sched_task *task2) {
    if (task1->deadline == task2->deadline) return sched_compare_task_by_prio(task1, task2);
    if (task1->deadline == 0) return -1;
    if (task2->deadline == 0) return 1;
    return task1->deadline < task2->deadline ? 1 : -1;
}

void sched_run(enum policy policy) {
    switch (policy) {
        case POLICY_FIFO:
            sched_do_run(sched_compare_task_by_fifo);
            break;
        case POLICY_PRIO:
            sched_do_run(sched_compare_task_by_prio);
            break;
        case POLICY_DEADLINE:
            sched_do_run(sched_compare_task_by_deadline);
            break;
        default:
            fprintf(stderr, "Unknown scheduler policy\n");
    }
}

void sched_time_elapsed(unsigned amount) {
    time += amount;
}
