#pragma once

// Returns number of microseconds (usec) since the latest interrupt
extern int timer_cnt(void);

// Initializes the timer to run hnd each time ms milliseconds passes
extern void timer_init(int ms, void (* hnd)(int sig, siginfo_t* info, void* ctx));
