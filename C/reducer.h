#ifndef REDUCER_H
#define REDUCER_H

#include "term.h"
#include "runtime.h"
#include <stdbool.h>

/* ============================================================
 * 转换策略
 * ============================================================ */

typedef enum {
    CONV_GREEDY,
    CONV_WHN,
    CONV_WHNFv2,
} ConvStrategy;

/* ============================================================
 * 归约器
 * ============================================================ */

typedef struct {
    GlobalContext* global_ctx;
    ConvStrategy strategy;
    uint32_t max_steps;
} Reducer;

/* 初始化归约器 */
void reducer_init(Reducer* r, GlobalContext* ctx, ConvStrategy strategy);

/* 弱头范式 */
Term* reducer_whnf(Reducer* r, const Term* term);

/* 弱头范式（不展开全局定义） */
Term* reducer_whnf_no_delta(Reducer* r, const Term* term);

/* 范式 */
Term* reducer_nf(Reducer* r, const Term* term);

/* 定义相等检查 */
bool reducer_is_def_eq(Reducer* r, const Term* lhs, const Term* rhs);

/* 单步归约（返回 NULL 表示无法归约） */
Term* reducer_step(Reducer* r, const Term* term);

#endif /* REDUCER_H */
