#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

#include "timer.h"

static struct timeval init_interval;

static void (* timer_hnd)(void);

static void signal_hnd(int sig, siginfo_t* info, void* ctx) {
    timer_hnd();
}

int timer_cnt(void) {
    struct itimerval timer_value;

    if (getitimer(ITIMER_REAL, &timer_value) == -1) {
        fprintf(stderr, "Failed to obtain a value of the timer");
        return -1;
    }

    long time_left = timer_value.it_value.tv_sec * 1000000 + timer_value.it_value.tv_usec;
    if (time_left == 0) {
        fprintf(stderr, "Cannot determine timer count: the timer is disarmed");
        return -1;
    }

    long interval = timer_value.it_interval.tv_sec * 1000000 + timer_value.it_interval.tv_usec;
    if (interval == 0) {
        fprintf(stderr, "Cannot determine timer count: the timer is single-shot");
        return -1;
    }

    return (int) (interval - time_left);
}

void timer_init(int ms, void (* hnd)(void)) {
    init_interval = (struct timeval) {0, ms * 1000};
    timer_hnd = hnd;

    const struct itimerval timer_value = {init_interval, init_interval};

    struct sigaction act = {
            .sa_sigaction = signal_hnd,
            .sa_flags = SA_RESTART | SA_SIGINFO,
    };
    sigemptyset(&act.sa_mask);

    if (setitimer(ITIMER_REAL, &timer_value, NULL) == -1) {
        fprintf(stderr, "Failed to setup the timer");
    }

    if (sigaction(SIGALRM, &act, NULL) == -1) {
        fprintf(stderr, "Failed to setup a handler for the timer");
    }
}
