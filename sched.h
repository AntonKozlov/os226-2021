#pragma once

// Pause execution for specified amount of msec
extern void sched_sleep(unsigned ms);

// milliseconds since system start
extern long sched_gettime(void);

// DANGEROUS: only for tests
extern void irq_disable(void);
extern void irq_enable(void);

