#ifndef CONTEXT_H
#define CONTEXT_H

#include "term.h"

/* ============================================================
 * 上下文 - 链表实现，O(1) extend/pop
 * ============================================================ */

typedef struct CtxNode {
    const Term* ty;
    const char* name;       /* 可选，用于调试 */
    struct CtxNode* tail;
} CtxNode;

typedef struct {
    CtxNode* head;
    uint32_t depth;
} Context;

/* 初始化空上下文 */
void ctx_init(Context* ctx);

/* 扩展上下文：O(1) */
void ctx_extend(Context* ctx, const char* name, const Term* ty);

/* 退出作用域：O(1) */
void ctx_pop(Context* ctx);

/* 查找变量：O(depth) */
const Term* ctx_lookup(Context* ctx, uint32_t index);

/* 获取深度 */
uint32_t ctx_depth(const Context* ctx);

/* 释放上下文（不释放 Term） */
void ctx_free(Context* ctx);

#endif /* CONTEXT_H */
