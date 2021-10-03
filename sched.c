#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "sched.h"
#include "pool.h"

static int time;

void sched_new(void (* entrypoint)(void* aspace),
               void* aspace,
               int priority,
               int deadline) {

}

void sched_cont(void (* entrypoint)(void* aspace),
                void* aspace,
                int timeout) {

}

void sched_time_elapsed(unsigned amount) {
    time += amount;

}

void sched_run(enum policy policy) {
}
