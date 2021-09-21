#pragma once

#include "util.h"

struct pool {

    long mbsz;

    void *m;
    void *fs;
    void *fe;

    struct fb { struct fb *k } *f;

};

#define POOL_INITIALIZER(_mem, _nmemb, _membsz) (struct pool) { \
                                                                \
    .m = (_mem),                                                \
    .f = NULL,                                                  \
    .fs = (_mem),                                               \
    .fe = ((void *)(_mem)) + (_nmemb) * (_membsz),              \
    .mbsz = (_membsz)                                           \
                                                                \
}

#define POOL_INITIALIZER_ARRAY(_array) POOL_INITIALIZER(_array, ARRAY_SIZE(_array), sizeof((_array)[0]));

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz);

void *pool_alloc(struct pool *p);

void pool_free(struct pool *p, void *ptr);
