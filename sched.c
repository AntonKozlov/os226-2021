#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "sched.h"
#include "pool.h"

static int time;
struct program {
    void (*entrypoint)(void *aspace);
    void *aspace;
    int priority;
    int deadline;
    int timeout;
    struct program *next;
};

struct program *current_pr = NULL;
struct program *executable_pr = NULL;

void add_pr(void (*entrypoint)(void *aspace),
            void *aspace,
            int priority,
            int deadline,
            int timeout){
    struct program *new = malloc(sizeof(struct program));
    *new = (struct program) {
            .entrypoint = entrypoint,
            .aspace = aspace,
            .priority = priority,
            .deadline = deadline,
            .next = NULL,
            .timeout = timeout
    };
    if (current_pr == NULL) current_pr = new;
    else {
        new->next = current_pr;
        current_pr = new;
    }
};

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline) {
    add_pr(entrypoint, aspace, priority, deadline, 0);
}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {
    add_pr(entrypoint, aspace, executable_pr->priority, executable_pr->deadline, timeout);
}

void sched_time_elapsed(unsigned amount) {
	time += amount;

}

void run(int (*run_policy)()) {
    while (current_pr != NULL) {
        struct program *prev_pr_for_exec = NULL, *prev_pr = NULL, *curr_pr = current_pr;
        if (current_pr->timeout == 0) executable_pr = current_pr;
        else executable_pr = NULL;
        while (curr_pr != NULL) {
            if (curr_pr->timeout != 0) curr_pr->timeout--;
            else if (executable_pr == NULL || run_policy(executable_pr, curr_pr) == 0) {
                prev_pr_for_exec = prev_pr;
                executable_pr = curr_pr;
            }
            if (prev_pr == NULL) prev_pr = curr_pr;
            else prev_pr = prev_pr->next;
            curr_pr = curr_pr->next;
        }
        if (executable_pr != NULL) executable_pr->entrypoint(executable_pr->aspace);
        if (prev_pr_for_exec == NULL) current_pr = current_pr->next;
        else prev_pr_for_exec->next = executable_pr->next;
    }
}

int run_fifo(struct program *pr1, struct program *pr2) {
    return 0;
}

int run_prio(struct program *pr1, struct program *pr2) {
    if (pr2->priority >= pr1->priority) return 0;
    else return 1;
}

int run_deadline(struct program *pr1, struct program *pr2) {
    if ((pr1->deadline - pr2->deadline == 0) || (pr1->deadline <= 0 && pr2->deadline <= 0)) return run_prio(pr1, pr2);
    else if (pr1->deadline > 0 && (pr2->deadline <= 0 || pr1->deadline - pr2->deadline < 0)) return 1;
    else if (pr2->deadline > 0 && (pr1->deadline <= 0 || pr2->deadline - pr1->deadline < 0)) return 0;
    else return -1;
}

void sched_run(enum policy policy) {
    if (policy == POLICY_FIFO) run(run_fifo);
    else if (policy == POLICY_PRIO) run(run_prio);
    else if (policy == POLICY_DEADLINE) run(run_deadline);
}
