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
    // TODO: getitimer
}

void timer_init(int ms, void (* hnd)(void)) {
    // TODO: setitimer
}
