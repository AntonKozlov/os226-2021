#pragma once

#include "util.h"

struct pool {
    void *mem;
    unsigned long membsz;
    void *free_mem_start_ptr;
    void *free_mem_end_ptr;
    struct pool_block {
        struct pool_block *next;
    } *free_block_ptr;
};

#define POOL_INITIALIZER(_mem, _nmemb, _membsz) (struct pool) {     \
    .mem = (_mem),                                                  \
    .membsz = (_membsz),                                            \
    .free_mem_start_ptr = (_mem),                                   \
    .free_mem_end_ptr = (((void*)(_mem)) + ((_nmemb) * (_membsz))), \
    .free_block_ptr = NULL}                                         \

#define POOL_INITIALIZER_ARRAY(_array) \
	POOL_INITIALIZER(_array, ARRAY_SIZE(_array), sizeof((_array)[0]));

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz);

void *pool_alloc(struct pool *p);

void pool_free(struct pool *p, void *ptr);
