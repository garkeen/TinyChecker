#include "context.h"
#include "core_ops.h"
#include <stdlib.h>
#include <assert.h>

/* ============================================================
 * 上下文实现
 * ============================================================ */

void ctx_init(Context* ctx) {
    ctx->head = NULL;
    ctx->depth = 0;
}

void ctx_extend(Context* ctx, const char* name, const Term* ty) {
    CtxNode* node = (CtxNode*)malloc(sizeof(CtxNode));
    assert(node != NULL);
    node->ty = ty;
    node->name = name;
    node->tail = ctx->head;
    ctx->head = node;
    ctx->depth++;
}

void ctx_pop(Context* ctx) {
    assert(ctx->head != NULL);
    CtxNode* old = ctx->head;
    ctx->head = old->tail;
    ctx->depth--;
    free(old);
}

const Term* ctx_lookup(Context* ctx, uint32_t index) {
    assert(index < ctx->depth);
    
    CtxNode* node = ctx->head;
    for (uint32_t i = 0; i < index; i++) {
        node = node->tail;
    }
    
    /* shift 以调整 de Bruijn index */
    return term_shift(node->ty, index + 1, 0);
}

uint32_t ctx_depth(const Context* ctx) {
    return ctx->depth;
}

void ctx_free(Context* ctx) {
    while (ctx->head != NULL) {
        CtxNode* old = ctx->head;
        ctx->head = old->tail;
        free(old);
    }
    ctx->depth = 0;
}
