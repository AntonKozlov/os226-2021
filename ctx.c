#include <string.h>

#include "ctx.h"

#define ENDBR64 0xfa1e0ff3

void ctx_make(struct ctx *ctx, void *entry, void *stack) {
        memset(ctx, 0, sizeof(*ctx));

        if(*(unsigned*) entry == ENDBR64 && (unsigned long) stack % 16 == 0) stack -= 8;
        ctx->rsp = (unsigned long) stack;
        ctx->rsp -= 8;
        *(unsigned long *)ctx->rsp = (unsigned long) entry;
}

