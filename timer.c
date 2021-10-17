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
    struct itimerval timer;
    getitimer(ITIMER_REAL, &timer);

    long time_left = timer.it_value.tv_sec * 1000 + timer.it_value.tv_usec / 1000;
    long interval = timer.it_interval.tv_sec * 1000 + timer.it_interval.tv_usec / 1000;

    return (interval - time_left);
}

void timer_init(int ms, void (*hnd)(void)) {
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ms * 1000;

    struct itimerval timer;
    timer.it_interval = tv;
    timer.it_value = tv;

    setitimer(ITIMER_REAL, &timer, NULL);
    signal(SIGALRM, hnd);
}
