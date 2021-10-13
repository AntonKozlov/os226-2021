#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "sched.h"
#include "pool.h"
static int time = 0;



struct s_tasks {
    void (*entrypoint)(void *aspace);
    void *aspace;
    int prio;
    int deadline;
    int time_border;
    struct s_tasks* next_t;
} tasks_arr[16];
struct pool tasks_pool = POOL_INITIALIZER_ARRAY(tasks_arr);
struct s_tasks *head = NULL;
struct s_tasks *now_task = NULL;

void new_task (void (*entrypoint)(void *aspace),
               void *aspace,
               int priority,
               int deadline,
               int time_border){
    struct s_tasks *task = pool_alloc(&tasks_pool);
    task->entrypoint = entrypoint;
    task->aspace = aspace;
    task->prio = priority;
    task->deadline = deadline;
    task->time_border = time_border;
    task->next_t = head;
    head = task;
}

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline) {
    new_task(entrypoint,aspace,priority,deadline,time);
}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {
    if (now_task == NULL)
        return;
    new_task(entrypoint,aspace,now_task->prio,now_task->deadline,time+timeout);
}

void sched_time_elapsed(unsigned amount) {
	time += amount;

}
void run (int (*cmp)(struct s_tasks* t1, struct s_tasks* t2)){

    while (head!=NULL){
        struct s_tasks** ch_task = NULL;
        for (struct s_tasks **pointer = &head; *pointer != NULL; pointer = &((*pointer)->next_t)) {
            if ((ch_task == NULL || cmp(*pointer,*ch_task) >= 0)&&(*pointer)->time_border<=time)
                ch_task = pointer;
        }
        if (ch_task!=NULL) {
            struct s_tasks *buf = *ch_task;
            *ch_task = buf->next_t;
            now_task = buf;
            now_task->entrypoint(now_task->aspace);
            now_task = NULL;
            pool_free(&tasks_pool, buf);
        }
    }

}

int cmp_fifo(struct s_tasks* t1, struct s_tasks* t2){
    return 0;
}

int cmp_prio(struct s_tasks* t1, struct s_tasks* t2){
    if (t1->prio>=t2->prio)
        return 1;
    else
        return -1;
}

int cmp_deadline (struct s_tasks* t1, struct s_tasks* t2){
    if (t1->deadline == t2->deadline)
        return cmp_prio(t1,t2);
    if (t1->deadline == -1) return -1;
    if (t2->deadline == -1) return 1;
    if (t1->deadline<t2->deadline)
        return 1;
    else
        return -1;
}

void sched_run(enum policy policy) {
    if (policy == POLICY_FIFO) run(cmp_fifo);
    else if (policy == POLICY_PRIO) run(cmp_prio);
    else if (policy == POLICY_DEADLINE) run(cmp_deadline);
}

