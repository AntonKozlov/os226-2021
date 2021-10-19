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
    struct itimerval timer_val;

    if (getitimer(ITIMER_REAL, &timer_val)) {
        fprintf(stderr, "An error occurred while getting the timer");
        exit(-1);
    }

    return   timer_val.it_interval.tv_usec
           + timer_val.it_interval.tv_sec * 1000000
           - timer_val.it_value.tv_usec
           - timer_val.it_value.tv_sec * 1000000;
}

void timer_init(int ms, void (*hnd)(void)) {
    struct timeval interval = {
            .tv_sec  = ms / 1000,
            .tv_usec = ms * 1000 % 1000
    };

    struct itimerval new_value = {
            .it_interval = interval,
            .it_value    = interval
    };

    if (setitimer(ITIMER_REAL, &new_value, NULL)) {
        fprintf(stderr, "An error occurred while setting the timer");
        exit(-1);
    }

    if (signal(SIGALRM, (__sighandler_t) hnd)) {
        fprintf(stderr, "An error occurred while setting the handler");
        exit(-1);
    }
}
