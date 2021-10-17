#pragma once

// Returns number of microseconds (usec) since the latests interrupt
extern int timer_cnt(void);

extern void timer_init(int ms, void (* hnd)(void));
