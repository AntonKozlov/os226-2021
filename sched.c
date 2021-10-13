#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sched.h"
#include "pool.h"

struct task
{
	void (*entry)(void *aspace);
	void *aspace;
	int priority;
	int deadline;
	int task_id;
	int time;

	struct task* next_run;
	struct task* next_wait;
};
struct task tasks[16];
int ntasks;

static int time;

struct task* run_list;
struct task* wait_list;

int (*policy_cmp)(struct task* t1, struct task* t2);
int fifo_cmp(struct task* t1, struct task* t2)
{
	return t1->task_id - t2->task_id;
}

int priority_cmp(struct task* t1, struct task* t2)
{
	if (t2->priority - t1->priority) return t2->priority - t1->priority;
	return fifo_cmp(t1, t2);
}

int deadline_cmp(struct task* t1, struct task* t2)
{
	if (t1->deadline - t2->deadline) return t1->deadline - t2->deadline;
	return priority_cmp(t1, t2);
}

void policy_sort()
{
	qsort(tasks, ntasks, sizeof(struct task), policy_cmp);
	if(policy_cmp == deadline_cmp)
	{
		while(tasks[0].deadline == 0)
		{
			struct task tmp = tasks[0];
			for(int i = 0; i < ntasks - 1; i++)
			{
				tasks[i] = tasks[i + 1];
			}
			tasks[ntasks - 1] = tmp;
		}
	}
}

void insert(struct task* tmp, struct task* task)
{
	if(policy_cmp == deadline_cmp && (tmp->deadline == 0 || task->deadline == 0))
	{
		if(task->deadline == 0)
		{
			struct task* tmp2 = run_list;
			while(tmp2)
			{
				if(!tmp2->next_run) break;
				tmp2 = tmp2->next_run;
			}
			tmp2->next_run = task;
			task->next_run = NULL;
		}
		else if(tmp->deadline == 0)
		{
			task->next_run = tmp;
			run_list = task;
		}
	}
	else
	{
		while(tmp && policy_cmp(tmp, task) < 0)
		{
			if(!tmp->next_run)  break;
			tmp = tmp->next_run;
		}
	}

	struct task* tmp2 = run_list;
	if(tmp2 != tmp)
	{
		while(tmp2->next_run)
		{
			if(tmp2->next_run == tmp) break;
			tmp2 = tmp2->next_run;
		}
		tmp2->next_run = task;
		task->next_run = tmp;
	}
	else
	{
		if(policy_cmp == fifo_cmp)
		{
			struct task* tmp3 = tmp2->next_run;
			tmp2->next_run = task;
			task->next_run = tmp3;
		}
		else if(policy_cmp == priority_cmp)
		{
			if(task->priority > tmp->priority)
			{
				task->next_run = tmp;
				run_list = task;
			}
			else if(task->priority < tmp->priority)
			{
				tmp->next_run = task;
				task->next_run = NULL;
			}
		}
	}
}

void priority_insert(struct task* tmp, struct task* task)
{
	if(tmp->priority - task->priority == 0)
	{
		policy_cmp = fifo_cmp;
		insert(tmp, task);
	}
	else insert(tmp, task);
}

void deadline_insert(struct task* tmp, struct task* task)
{
	if(tmp->deadline - task->deadline == 0)
	{
		policy_cmp = priority_cmp;
		priority_insert(tmp, task);
	}
	else insert(tmp, task);
}

void insert_run_pool(struct task* task)
{
	if(run_list)
	{
		struct task* tmp = run_list;
		if(policy_cmp == fifo_cmp) insert(tmp, task);
		else if(policy_cmp == priority_cmp) priority_insert(tmp, task);
		else if(policy_cmp == deadline_cmp) deadline_insert(tmp, task);
	}
	else run_list = task;
}

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline)
{
	struct task *t = &tasks[ntasks];
	t->entry = entrypoint;
	t->aspace = aspace;
	t->priority = priority;
	t->deadline = deadline > 0 ? deadline : 0;
	t->task_id = ntasks++;
	t->time = 0;
}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout)
{
	if(timeout > 0)
	{
		for(int i = 0; i < ntasks; i++)
		{
			if(tasks[i].aspace == aspace)
			{
				tasks[i].time = time + timeout;
				if(!wait_list)
				{
					wait_list = &tasks[i];
					wait_list->next_run = NULL;
				}
				else
				{
					struct task* tmp = &tasks[i];
					tmp->next_run = NULL;
					struct task* tmp2 = wait_list;

					for(;;)
					{
						if(!tmp2->next_wait) break;
						tmp2 = tmp2->next_wait;
					}
					tmp2->next_wait = tmp;
				}
				break;
			}
		}
	}
	else
	{
		for(int i = 0; i < ntasks; i++)
		{
			if(tasks[i].aspace == aspace)
			{
				insert_run_pool(&tasks[i]);
				break;
			}
		}
	}
}

void sched_time_elapsed(unsigned amount)
{
	time += amount;
}

void sched_start(void)
{
	policy_sort();
	for(int i = 0; i < ntasks - 1; i++) tasks[i].next_run = &tasks[i + 1];
	run_list = &tasks[0];
	while(run_list)
	{
		struct task* run_list_head = run_list;
		run_list = run_list->next_run;
		run_list_head->entry(run_list_head->aspace);
	}
}

void sched_run(enum policy policy)
{
	switch (policy)
	{
		case POLICY_FIFO:
		{
			policy_cmp = fifo_cmp;
			sched_start();
			break;
		}
		case POLICY_PRIO:
		{
			policy_cmp = priority_cmp;
			sched_start();
			break;
		}
		case POLICY_DEADLINE:
		{
			policy_cmp = deadline_cmp;
			sched_start();
			break;
		}
		default:
		{
			printf("Unknown policy");
			break;
		}
	}
}
