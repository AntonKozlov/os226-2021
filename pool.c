#include <stddef.h>

#include "pool.h"

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz) {
    *p = POOL_INITIALIZER(mem, nmemb, membsz);
}

void *pool_alloc(struct pool *p) {
    struct pool_obj *po = p->free;
    if (po) {
        p->free = po->next;
        return po;
    }
    if (p->start < p->end) {
        struct pool_obj *r = p->start;
        p->start += p->membsz;
        return r;
    }
    return NULL;
}

void pool_free(struct pool *p, void *ptr) {
    struct pool_obj *po = ptr;
    po->next = p->free;
    p->free = po;
}