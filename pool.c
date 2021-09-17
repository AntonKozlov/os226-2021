#include <stddef.h>

#include "pool.h"

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz) {
    *p = POOL_INITIALIZER(mem, nmemb, membsz);
}

int allocated_in_pool(struct pool *p, void *ptr) {
    return p-> mem <= ptr && ptr <= p->free_mem_end_ptr;
}

int pool_contains_free_blocks(struct pool *p) {
    return p->free_mem_start_ptr < p->free_mem_end_ptr;
}

void *pool_alloc(struct pool *p) {
    if (p->free_block_ptr) {
        struct pool_block *free_block_ptr = p->free_block_ptr;
        p->free_block_ptr = free_block_ptr->next;
        return free_block_ptr;
    }

    if (pool_contains_free_blocks(p)) {
        struct pool_block* free_block_ptr = p->free_mem_start_ptr;
        p->free_mem_start_ptr += p->membsz;
        return free_block_ptr;
    }
	return NULL;
}

void pool_free(struct pool *p, void *ptr) {
    if (allocated_in_pool(p, ptr)) {
        struct pool_block *new_free_block = ptr;
        *new_free_block = (struct pool_block) {
            .next = p->free_block_ptr
        };
        p->free_block_ptr = new_free_block;
    }
}
