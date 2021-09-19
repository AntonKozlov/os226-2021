
#include <stddef.h>

#include "pool.h"

void pool_init(struct pool *p, void *mem, unsigned long nmemb, unsigned long membsz) {
	p->free_block = NULL;
	p->mem = mem;
	p->free_begin = mem;
	p->free_end = mem + nmemb * membsz;
	p->nmemb = nmemb;
	p->membsz = membsz;
}

void *pool_alloc(struct pool *p) {
	if(p->free_begin < p->free_end) {
		void* tmp = p->free_begin;
		p->free_begin += p->membsz;

		return tmp;
	}

	if(p->free_block) {
		struct block* tmp = p->free_block;
		p->free_block = tmp->next;

		return tmp;
	}

	return NULL;
}

void pool_free(struct pool *p, void *ptr) {
	if(ptr >= p->mem && ptr <= p->free_end) {
		struct block* tmp = ptr;
		tmp->next = p->free_block;
		p->free_block = tmp;
	}
}
