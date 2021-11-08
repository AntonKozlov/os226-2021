#include <string.h>

#include "ctx.h"

void ctx_make(struct ctx* ctx, void* entry, void* stack) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->rsp = (unsigned long) stack;
    ctx->rsp -= 8;
    *(unsigned long*) ctx->rsp = (unsigned long) entry;
}
