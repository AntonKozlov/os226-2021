
#include <stddef.h>

#include "pool.h"

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz) {

    *p = POOL_INITIALIZER(mem, nmemb, membsz);

}

void *pool_alloc(struct pool *p) {

    struct fb *fb = p -> f;

    if (fb) {

        p -> f = fb -> k;

        return fb;

    }

    else if (p -> fs < p -> fe) {

        void *r = p -> fs;

        p -> fs += p -> mbsz;

        return r;

    }

    return NULL;

}

void pool_free(struct pool *p, void *ptr) {

    struct fb *fb = ptr;

    *fb = (struct fb) {.k = p -> f};

    p -> f = fb;

}