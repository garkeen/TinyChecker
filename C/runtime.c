#include "runtime.h"
#include <string.h>
#include <assert.h>

/* ============================================================
 * 全局上下文实现
 * ============================================================ */

void global_ctx_init(GlobalContext* ctx) {
    ctx->global_count = 0;
    ctx->inductive_count = 0;
    ctx->constructor_count = 0;
    ctx->eliminator_count = 0;
}

bool global_ctx_register(GlobalContext* ctx, GlobalEntry entry) {
    if (ctx->global_count >= MAX_GLOBALS) {
        return false;
    }
    /* 检查是否已存在 */
    if (entry.name != NULL) {
        for (uint32_t i = 0; i < ctx->global_count; i++) {
            if (ctx->globals[i].name != NULL && strcmp(ctx->globals[i].name, entry.name) == 0) {
                return false;  /* 重复注册 */
            }
        }
    }
    ctx->globals[ctx->global_count++] = entry;
    return true;
}

GlobalEntry* global_ctx_lookup(GlobalContext* ctx, const char* name) {
    if (name == NULL) return NULL;
    for (uint32_t i = 0; i < ctx->global_count; i++) {
        if (ctx->globals[i].name != NULL && strcmp(ctx->globals[i].name, name) == 0) {
            return &ctx->globals[i];
        }
    }
    return NULL;
}

bool global_ctx_register_inductive(GlobalContext* ctx, InductiveInfo info) {
    if (ctx->inductive_count >= MAX_INDUCTIVES) {
        return false;
    }
    ctx->inductives[ctx->inductive_count++] = info;
    return true;
}

InductiveInfo* global_ctx_lookup_inductive(GlobalContext* ctx, const char* name) {
    if (name == NULL) return NULL;
    for (uint32_t i = 0; i < ctx->inductive_count; i++) {
        if (ctx->inductives[i].name != NULL && strcmp(ctx->inductives[i].name, name) == 0) {
            return &ctx->inductives[i];
        }
    }
    return NULL;
}

bool global_ctx_register_constructor(GlobalContext* ctx, ConstructorInfo info) {
    if (ctx->constructor_count >= MAX_CONSTRUCTORS) {
        return false;
    }
    ctx->constructors[ctx->constructor_count++] = info;
    return true;
}

ConstructorInfo* global_ctx_lookup_constructor(GlobalContext* ctx, const char* name) {
    if (name == NULL) return NULL;
    for (uint32_t i = 0; i < ctx->constructor_count; i++) {
        if (ctx->constructors[i].name != NULL && strcmp(ctx->constructors[i].name, name) == 0) {
            return &ctx->constructors[i];
        }
    }
    return NULL;
}

bool global_ctx_register_eliminator(GlobalContext* ctx, EliminatorInfo info) {
    if (ctx->eliminator_count >= MAX_ELIMINATORS) {
        return false;
    }
    ctx->eliminators[ctx->eliminator_count++] = info;
    return true;
}

EliminatorInfo* global_ctx_lookup_eliminator(GlobalContext* ctx, const char* name) {
    if (name == NULL) return NULL;
    for (uint32_t i = 0; i < ctx->eliminator_count; i++) {
        if (ctx->eliminators[i].name != NULL && strcmp(ctx->eliminators[i].name, name) == 0) {
            return &ctx->eliminators[i];
        }
    }
    return NULL;
}
