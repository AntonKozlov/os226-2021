#include <stddef.h>
#include <stdio.h>

#include "pool.h"

void pool_init(struct pool* p, void* mem, unsigned long nmemb, unsigned long membsz) {
    *p = (struct pool) POOL_INITIALIZER(mem, nmemb, membsz);
}

void* pool_alloc(struct pool* p) {
    if (p->next == p->end) {
        fprintf(stderr, "Allocation failed: pool %p is full\n", p->base);
        return NULL;
    }

    void* r = p->next;
    p->next += p->size;
    return r;
}

void pool_free(struct pool* p, void* ptr) {
    if (ptr < p->base || ptr >= p->next || (ptr - p->base) % p->size != 0) {
        fprintf(stderr, "Freeing failed: %p is not from pool %p\n", ptr, p->base);
        return;
    }

    while (p->next != ptr) {
        p->next -= p->size;
    }
}
