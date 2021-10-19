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
    int time_interval = timer.it_interval.tv_sec * 1000000 + timer.it_interval.tv_usec;
    int time_value = timer.it_value.tv_sec * 1000000 + timer.it_value.tv_usec;

    return (time_interval - time_value);
}

void timer_init(int ms, void (*hnd)(void)) {
    struct sigaction sa;
    struct itimerval timer;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = hnd;

    sigaction(SIGALRM, &sa, NULL);

    timer.it_value.tv_sec = ms / 1000;
    timer.it_value.tv_usec = (ms % 1000) * 1000;
    timer.it_interval.tv_sec = ms / 1000;
    timer.it_interval.tv_usec = (ms % 1000) * 1000;

    setitimer(ITIMER_REAL, &timer, NULL);
}
