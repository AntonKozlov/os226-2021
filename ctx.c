#include <string.h>

#include "ctx.h"

void ctx_make(struct ctx *ctx, void *entry, void *stack, int alignment) {
        memset(ctx, 0, sizeof(*ctx));

        if(alignment == STANDARD) stack = (void*)(((unsigned long)stack & ~0xf) - 8);
        ctx->rsp = (unsigned long) stack;
        ctx->rsp -= 8;
        *(unsigned long *)ctx->rsp = (unsigned long) entry;
}

