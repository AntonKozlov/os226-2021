#pragma once

struct ctx {
	unsigned long rbx;
	unsigned long r12;
	unsigned long r13;
	unsigned long r14;
	unsigned long r15;
	unsigned long rsp;
	unsigned long rbp;
	unsigned long rip;
};

extern void ctx_make(struct ctx *ctx, void *entry, void *stack, int stacksz);

extern void ctx_switch(struct ctx *old, struct ctx *new);

