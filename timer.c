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

    long interval = timer.it_interval.tv_sec * 1000 + timer.it_interval.tv_usec / 1000;
    long time_left = timer.it_value.tv_sec * 1000 + timer.it_value.tv_usec / 1000;

    return (interval - time_left);
}

void timer_init(int ms, void (*hnd)(void)) {
    struct timeval interval = {
            .tv_sec = ms / 1000,
            .tv_usec = ms * 1000
    };

    struct itimerval new_value = {
            .it_interval = interval,
            .it_value = interval
    };

    setitimer(ITIMER_REAL, &new_value, NULL);
    signal(SIGALRM, hnd);
}
