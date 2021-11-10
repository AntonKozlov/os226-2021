#define _GNU_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "timer.h"

static struct timeval initv;

int timer_cnt(void) {
	struct itimerval it;
	getitimer(ITIMER_REAL, &it);
	return 1000000 * (initv.tv_sec - it.it_value.tv_sec)
		+ (initv.tv_usec - it.it_value.tv_usec);
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
