#define _GNU_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "timer.h"

int timer_cnt(void) {
    struct itimerval value_of_timer;
    getitimer(ITIMER_REAL, &value_of_timer);
    return value_of_timer.it_interval.tv_sec * 1000000 + value_of_timer.it_interval.tv_usec -
            value_of_timer.it_value.tv_sec * 1000000 - value_of_timer.it_value.tv_usec;
}

void timer_init(int ms, void (*hnd)(void)) {
    struct timeval timer_interval;
    struct itimerval timer;
    timer_interval.tv_sec = ms / 1000;
    timer_interval.tv_usec = (ms % 1000) * 1000;
    timer.it_interval = timer_interval;
    timer.it_value = timer_interval;
    setitimer(ITIMER_REAL, &timer, NULL);
    signal(SIGALRM, hnd);
}
