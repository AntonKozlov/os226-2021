#include <string.h>

#include "ctx.h"

void ctx_make(struct ctx *ctx, void *entry, void *stack, int stacksz) {
        memset(ctx, 0, sizeof(*ctx));

        ctx->rsp = (unsigned long) stack + stacksz - 16;
        ctx->rsp = ctx->rsp;
        ctx->rsp -= 8;
        *(unsigned long *)ctx->rsp = (unsigned long) entry;
}

