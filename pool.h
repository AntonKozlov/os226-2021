#pragma once

#include "util.h"

struct pool {
    void *mem;
    unsigned long membsz;
    void *freestart;
    void *freeend;
    struct pool_free_block {
        struct pool_free_block *next;
    } *free;
};

#define POOL_INITIALIZER(_mem, _nmemb, _membsz) { \
    .mem = _mem,                                  \
    .membsz = _membsz,                            \
    .freestart = _mem,                            \
    .freeend = _nmemb * _membsz + (void*)_mem,    \
    .free = NULL                                  \
}

#define POOL_INITIALIZER_ARRAY(_array) \
	POOL_INITIALIZER(_array, ARRAY_SIZE(_array), sizeof((_array)[0]));

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz);

void *pool_alloc(struct pool *p);

void pool_free(struct pool *p, void *ptr);
