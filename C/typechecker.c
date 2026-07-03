#include "typechecker.h"
#include "core_ops.h"
#include "pretty.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ============================================================
 * 初始化
 * ============================================================ */

void typechecker_init(TypeChecker* tc, GlobalContext* ctx, ConvStrategy strategy) {
    tc->global_ctx = ctx;
    tc->strategy = strategy;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

static Reducer make_reducer(TypeChecker* tc) {
    Reducer r;
    reducer_init(&r, tc->global_ctx, tc->strategy);
    return r;
}

/* ============================================================
 * infer - 类型推导
 * ============================================================ */

Term* typechecker_infer(TypeChecker* tc, const Term* term, Context* ctx) {
    if (term == NULL) {
        return NULL;
    }
    
    Reducer reducer = make_reducer(tc);
    
    switch (term->kind) {
        case TERM_TYPE:
            /* Type : Type */
            return term_new_type();
        
        case TERM_VAR:
            /* 变量：从上下文查找类型 */
            return (Term*)ctx_lookup(ctx, term->var.index);
        
        case TERM_GLOBAL: {
            /* 全局名：从全局上下文查找类型 */
            GlobalEntry* entry = global_ctx_lookup(tc->global_ctx, term->global);
            if (entry == NULL) {
                /* 未找到全局名，返回 NULL 表示错误 */
                return NULL;
            }
            return (Term*)entry->typ;
        }
        
        case TERM_PI:
            /* (x: A) -> B : Type，如果 A: Type 且 B: Type */
            typechecker_check(tc, term->pi.domain, term_new_type(), ctx);
            ctx_extend(ctx, term->pi.name, term->pi.domain);
            typechecker_check(tc, term->pi.codomain, term_new_type(), ctx);
            ctx_pop(ctx);
            return term_new_type();
        
        case TERM_LAM: {
            /* \x: A. t : (x: A) -> B，如果 t: B 在 x: A 下 */
            typechecker_check(tc, term->lam.param_type, term_new_type(), ctx);
            ctx_extend(ctx, term->lam.name, term->lam.param_type);
            Term* body_ty = typechecker_infer(tc, term->lam.body, ctx);
            ctx_pop(ctx);
            return term_new_pi(term->lam.name, term->lam.param_type, body_ty);
        }
        
        case TERM_APP: {
            /* f a : B[x := a]，如果 f : (x: A) -> B 且 a : A */
            Term* func_ty = typechecker_infer(tc, term->app.func, ctx);
            if (func_ty == NULL) {
                return NULL;
            }
            func_ty = reducer_whnf(&reducer, func_ty);
            
            if (func_ty->kind != TERM_PI) {
                /* 非函数应用，返回 Type 作为占位 */
                return term_new_type();
            }
            
            typechecker_check(tc, term->app.arg, func_ty->pi.domain, ctx);
            return term_instantiate(func_ty->pi.codomain, term->app.arg);
        }
    }
    
    return term_new_type();  /* 默认返回 Type */
}

/* ============================================================
 * check - 类型检查
 * ============================================================ */

bool typechecker_check(TypeChecker* tc, const Term* term, const Term* expected, Context* ctx) {
    Reducer reducer = make_reducer(tc);
    
    /* 调试：检查 expected 是否有效 */
    if (expected == NULL) {
        return false;
    }
    
    Term* expected_whnf = reducer_whnf(&reducer, expected);
    
    /* 调试：检查 expected_whnf 是否有效 */
    if (expected_whnf == NULL) {
        return false;
    }
    
    /* 如果是 lambda，检查是否期望 Pi 类型 */
    if (term->kind == TERM_LAM) {
        if (expected_whnf->kind != TERM_PI) {
            return false;
        }
        
        /* 检查参数类型 */
        typechecker_check(tc, term->lam.param_type, term_new_type(), ctx);
        if (!typechecker_convert(tc, term->lam.param_type, expected_whnf->pi.domain)) {
            return false;
        }
        
        /* 检查 body */
        ctx_extend(ctx, term->lam.name, expected_whnf->pi.domain);
        bool result = typechecker_check(tc, term->lam.body, expected_whnf->pi.codomain, ctx);
        ctx_pop(ctx);
        return result;
    }
    
    /* 否则，推导类型并检查是否相等 */
    Term* inferred = typechecker_infer(tc, term, ctx);
    return typechecker_convert(tc, inferred, expected);
}

/* ============================================================
 * convert - 类型相等检查
 * ============================================================ */

bool typechecker_convert(TypeChecker* tc, const Term* lhs, const Term* rhs) {
    Reducer reducer = make_reducer(tc);
    return reducer_is_def_eq(&reducer, lhs, rhs);
}

/* ============================================================
 * 声明级注册函数
 * ============================================================ */

bool typechecker_register_definition(TypeChecker* tc, const char* name, const Term* typ, const Term* value, const char* kind) {
    GlobalKind gkind;
    if (strcmp(kind, "var") == 0) gkind = GLOBAL_VAR;
    else if (strcmp(kind, "fun") == 0) gkind = GLOBAL_FUN;
    else if (strcmp(kind, "theorem") == 0) gkind = GLOBAL_THEOREM;
    else if (strcmp(kind, "claim") == 0) gkind = GLOBAL_CLAIM;
    else if (strcmp(kind, "axiom") == 0) gkind = GLOBAL_AXIOM;
    else gkind = GLOBAL_VAR;
    
    return global_ctx_register(tc->global_ctx, (GlobalEntry){
        .name = name,
        .typ = typ,
        .value = value,
        .kind = gkind
    });
}

bool typechecker_register_type_ctor(TypeChecker* tc, const char* name, const char* kind,
    char** param_names, const Term** param_types, uint32_t param_count,
    char** index_names, const Term** index_types, uint32_t index_count,
    const char** ctor_names, uint32_t ctor_count) {
    
    InductiveInfo info = {
        .name = name,
        .kind = kind,
        .param_count = param_count,
        .index_count = index_count,
        .param_names = param_names,
        .param_types = param_types,
        .index_names = index_names,
        .index_types = index_types,
        .constructor_names = ctor_names,
        .constructor_count = ctor_count,
        .eliminator_name = NULL
    };
    
    return global_ctx_register_inductive(tc->global_ctx, info);
}

bool typechecker_register_data_ctor(TypeChecker* tc, const char* name, const char* owner,
    const Term* typ, uint32_t param_count,
    char** field_names, const Term** field_types, uint32_t field_count) {
    
    ConstructorInfo info = {
        .name = name,
        .owner = owner,
        .typ = typ,
        .param_count = param_count,
        .field_names = field_names,
        .field_types = field_types,
        .field_count = field_count,
        .recursive_fields = NULL,
        .recursive_field_count = 0,
        .target_args = NULL,
        .target_arg_count = 0
    };
    
    return global_ctx_register_constructor(tc->global_ctx, info);
}

bool typechecker_register_eliminator(TypeChecker* tc, const char* name, const char* owner,
    const Term* typ, const char** branch_order, uint32_t branch_count, const char* kind) {
    
    EliminatorInfo info = {
        .name = name,
        .owner = owner,
        .typ = typ,
        .branch_order = branch_order,
        .branch_count = branch_count,
        .kind = kind
    };
    
    return global_ctx_register_eliminator(tc->global_ctx, info);
}
