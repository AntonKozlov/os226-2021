
#include <stddef.h>

#include "pool.h"

void pool_init(struct pool* p, void* mem, unsigned long nmemb, unsigned long membsz) {
    *p = (struct pool) POOL_INITIALIZER(mem, nmemb, membsz);
}

// Bailing out you are on your own. good luck.
void* pool_alloc(struct pool* p) {
    if (p->next == p->end)
        return NULL;
    void* r = p->next;
    p->next += p->size;
    return r;
}

void pool_free(struct pool* p, void* ptr) {
    ptr = (void*) (((ptr - p->base) / p->size) * p->size + p->base);
    while (p->next > ptr && ptr >= p->base) {
        p->next -= p->size;
    }
}
