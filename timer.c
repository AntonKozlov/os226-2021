#define _GNU_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "timer.h"

static int get_usec_total(struct timeval *timeval) {
	return 1000000 * timeval->tv_usec + timeval->tv_sec;
}

int timer_cnt(void) {
	struct itimerval itimerval;
	getitimer(ITIMER_REAL, &itimerval);
	return get_usec_total(&itimerval.it_interval) - get_usec_total(&itimerval.it_value);
}

void timer_init(int ms, void (*hnd)(void)) {
	struct sigaction alrm_action = {
			.sa_handler = hnd
	};
	sigaction(SIGALRM, &alrm_action, NULL);
	struct timeval period = {
			.tv_sec = ms / 1000,
			.tv_usec = (ms % 1000) * 1000
	};
	struct itimerval itimerval = {
			.it_value = period,
			.it_interval = period
	};
	setitimer(ITIMER_REAL, &itimerval, NULL);
}
