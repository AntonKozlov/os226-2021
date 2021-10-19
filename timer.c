#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include "timer.h"

int timer_cnt(void) {
    struct itimerval timer;
    if (getitimer(ITIMER_REAL, &timer)) {
        fprintf(stderr, "Get timer error");
        return -1;
    }
    return (timer.it_interval.tv_sec - timer.it_value.tv_sec) * 1000000 +
           (timer.it_interval.tv_usec - timer.it_value.tv_usec);
}

void timer_init(int ms, void (*hnd)(void)) {
    struct timeval interval = {0, ms * 1000};
    struct itimerval timer = {interval, interval};
    if (setitimer(ITIMER_REAL, &timer, NULL) || signal(SIGALRM, (__sighandler_t) hnd))
        fprintf(stderr, "Set timer error");
}
