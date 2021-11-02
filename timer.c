#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include "timer.h"

static struct timeval initv;

int timer_cnt(void) {
    struct itimerval timer;
    if (getitimer(ITIMER_REAL, &timer)) {
        fprintf(stderr, "Get timer error");
        return -1;
    }
    return 1000000 * (initv.tv_sec - timer.it_value.tv_sec)
           + (initv.tv_usec - timer.it_value.tv_usec);
}

void timer_init(int ms, void (*hnd)(int sig, siginfo_t *info, void *ctx)) {

	initv.tv_sec  = ms / 1000;
	initv.tv_usec = (ms % 1000) * 1000;

	const struct itimerval setup_it = {
		.it_value    = initv,
		.it_interval = initv,
	};

	if (-1 == setitimer(ITIMER_REAL, &setup_it, NULL)) {
		perror("setitimer");
	}

	struct sigaction act = {
		.sa_sigaction = hnd,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGALRM, &act, NULL)) {
		perror("signal set failed");
		exit(1);
	}
}
