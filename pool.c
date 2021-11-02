#include <stddef.h>

#include "pool.h"

void pool_init(struct pool* p, void* mem, unsigned long nmemb, unsigned long membsz) {
    *p = (struct pool) POOL_INITIALIZER(mem, nmemb, membsz);
}

void* pool_alloc(struct pool* p) {
    struct pool_free_block* fb = p->freehead;
    if (fb) {
        p->freehead = fb->next;
        return fb;
    } else if (p->freestart < p->freeend) {
        void* r = p->freestart;
        p->freestart += p->membsz;
        return r;
    }

    return NULL;
}

void pool_free(struct pool* p, void* ptr) {
    struct pool_free_block* fb = ptr;
    fb->next = p->freehead;
    p->freehead = fb;
}
