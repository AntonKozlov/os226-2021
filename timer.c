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
	struct itimerval curr_value;
    getitimer(ITIMER_REAL, &curr_value);
    return (curr_value.it_interval.tv_usec + curr_value.it_interval.tv_sec * 1000000) - (curr_value.it_value.tv_usec + curr_value.it_value.tv_sec * 1000000);
}

void timer_init(int ms, void (*hnd)(void)) {
    struct timeval interval = {
            .tv_sec = (ms * 1000) / 1000000,
            .tv_usec = (ms * 1000) % 1000000
    };
    struct itimerval new_value = {
            .it_interval = interval,
            .it_value = interval
    };
    struct sigaction act = {
            .sa_handler = hnd
    };
    if(sigaction(SIGALRM, &act, NULL) != 0) {
        fprintf(stderr, "Cannot set sigaction in timer_init()");
        exit(-1);
    }
    setitimer(ITIMER_REAL, &new_value, NULL);
}