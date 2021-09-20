#pragma once

#include "util.h"

struct pool_obj {
    struct pool_obj *next;
};

struct pool {
    void *mem;
    unsigned long membsz;
    struct pool_obj *free;
    void *start;
    void *end;
};

#define POOL_INITIALIZER(_mem, _nmemb, _membsz) (struct pool) { \
    .mem = (_mem),                                              \
    .membsz = (_membsz),                                        \
    .free = NULL,                                               \
    .start = (_mem),                                            \
    .end = (void *)(_mem) + (_nmemb) * (_membsz)                \
}

#define POOL_INITIALIZER_ARRAY(_array) \
	POOL_INITIALIZER(_array, ARRAY_SIZE(_array), sizeof((_array)[0]));

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz);

void *pool_alloc(struct pool *p);

void pool_free(struct pool *p, void *ptr);