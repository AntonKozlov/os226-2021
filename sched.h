#pragma once

enum policy {
	// first-in, first-out; run tasks in order of their arrival
	POLICY_FIFO,

	// highest priority task (highest priority value) should be executed
	// first. Use round-robin for processes with same priority
	// (task from 1st process, from 2nd, ... Nth, 1st, 2nd, ...)
	POLICY_PRIO,

	// consider deadline, execute process with Earliest Deadline First.
	// Fallback to priority policy if deadlines are equal
	POLICY_DEADLINE,
};

// Add new task
extern void sched_new(void (*entrypoint)(void *ctx), // entrypoint function
		void *ctx,     // context of the process
		int priority); // priority, [0 - 10], bigger for more priority

// Pause execution for specified amount of msec
extern void sched_sleep(unsigned ms);

// Scheduler loop, start executing tasks until all of them finish
extern void sched_run(enum policy policy);

// milliseconds since system start
extern long sched_gettime(void);

// DANGEROUS: only for tests
extern void irq_disable(void);
extern void irq_enable(void);

