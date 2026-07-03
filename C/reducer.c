#include "reducer.h"
#include "core_ops.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================
 * 初始化
 * ============================================================ */

void reducer_init(Reducer* r, GlobalContext* ctx, ConvStrategy strategy) {
    r->global_ctx = ctx;
    r->strategy = strategy;
    r->max_steps = 200000;
}

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/* 尝试 beta 归约：(\x. body) arg -> body[x := arg] */
static Term* try_beta(const Term* head, const Term** args, size_t arg_count) {
    if (head->kind == TERM_LAM && arg_count > 0) {
        Term* body = term_instantiate(head->lam.body, args[0]);
        if (arg_count > 1) {
            return term_mk_apps(body, args + 1, arg_count - 1);
        }
        return body;
    }
    return NULL;
}

/* 尝试 delta 归约：展开全局定义 */
static Term* try_delta(Reducer* r, const char* name, const Term** args, size_t arg_count, bool allow_delta) {
    if (!allow_delta) return NULL;
    GlobalEntry* entry = global_ctx_lookup(r->global_ctx, name);
    if (entry == NULL || entry->value == NULL) {
        return NULL;
    }
    if (arg_count > 0) {
        return term_mk_apps(entry->value, args, arg_count);
    }
    return (Term*)entry->value;
}

/* 查看一个项是否为某个归纳类型的构造器应用 */
static ConstructorInfo* view_constructor(Reducer* r, const Term* term, const char* inductive_name) {
    if (term == NULL || inductive_name == NULL) return NULL;
    
    AppChain chain = term_unfold_app(term);
    
    if (chain.head->kind != TERM_GLOBAL) {
        app_chain_free(&chain);
        return NULL;
    }
    
    ConstructorInfo* info = global_ctx_lookup_constructor(r->global_ctx, chain.head->global);
    if (info == NULL || info->owner == NULL || strcmp(info->owner, inductive_name) != 0) {
        app_chain_free(&chain);
        return NULL;
    }
    
    app_chain_free(&chain);
    return info;
}

/* 尝试 iota 归约：对归纳类型的消去子进行模式匹配 */
static Term* try_iota(Reducer* r, const char* head_name, const Term** args, size_t arg_count) {
    EliminatorInfo* elim = global_ctx_lookup_eliminator(r->global_ctx, head_name);
    if (elim == NULL) return NULL;
    InductiveInfo* ind = global_ctx_lookup_inductive(r->global_ctx, elim->owner);
    if (ind == NULL) return NULL;
    size_t branch_count = elim->branch_count;
    size_t arity = ind->param_count + 1 + branch_count + ind->index_count + 1;
    if (arg_count < arity) return NULL;
    const Term** params = args;
    const Term* motive = args[ind->param_count];
    const Term** branches = args + ind->param_count + 1;
    const Term* scrutinee = args[arity - 1];
    const Term** extra = args + arity;
    size_t extra_count = arg_count - arity;
    
    Term* scrutinee_whnf = reducer_whnf(r, scrutinee);
    if (scrutinee_whnf == NULL) return NULL;
    ConstructorInfo* ctor_info = view_constructor(r, scrutinee_whnf, elim->owner);
    if (ctor_info == NULL) return NULL;
    AppChain scrut_chain = term_unfold_app(scrutinee_whnf);
    if (ctor_info->param_count > scrut_chain.count) {
        app_chain_free(&scrut_chain);
        return NULL;
    }
    const Term** field_args = scrut_chain.args + ctor_info->param_count;
    size_t field_count = scrut_chain.count - ctor_info->param_count;
    size_t branch_index = 0;
    for (size_t i = 0; i < branch_count; i++) {
        if (elim->branch_order[i] != NULL && ctor_info->name != NULL &&
            strcmp(elim->branch_order[i], ctor_info->name) == 0) {
            branch_index = i;
            break;
        }
    }
    const Term* branch = branches[branch_index];
    
    const Term* ihs[64];
    size_t ih_count = 0;
    
    for (size_t i = 0; i < ctor_info->recursive_field_count; i++) {
        RecursiveField* rec = &ctor_info->recursive_fields[i];
        const Term* field_value = field_args[rec->field_position];
        
        const Term* prefix_args[64];
        size_t prefix_count = 0;
        for (size_t j = 0; j < ind->param_count; j++) prefix_args[prefix_count++] = params[j];
        for (size_t j = 0; j < rec->field_position && j < field_count; j++) prefix_args[prefix_count++] = field_args[j];
        
        const Term* prefix_env[64];
        for (size_t j = 0; j < prefix_count; j++) prefix_env[j] = prefix_args[prefix_count - 1 - j];
        
        const Term* target_args[64];
        size_t target_arg_count = 0;
        /* 与 Python 一致：跳过前 param_count 个，对剩余的做 instantiate_env */
        for (size_t j = ind->param_count; j < rec->recursive_target_arg_count; j++) {
            target_args[target_arg_count++] = term_instantiate_env(
                rec->recursive_target_args[j], prefix_env, prefix_count, 0);
        }
        
        if (rec->ho_telescope_count == 0) {
            const Term* ih_args[256];
            size_t ih_arg_count = 0;
            for (size_t j = 0; j < ind->param_count; j++) ih_args[ih_arg_count++] = params[j];
            ih_args[ih_arg_count++] = motive;
            for (size_t j = 0; j < branch_count; j++) ih_args[ih_arg_count++] = branches[j];
            for (size_t j = 0; j < target_arg_count; j++) ih_args[ih_arg_count++] = target_args[j];
            ih_args[ih_arg_count++] = field_value;
            
            Term* ih = term_new_global(elim->name);
            for (size_t j = 0; j < ih_arg_count; j++) ih = term_new_app(ih, ih_args[j]);
            ihs[ih_count++] = ih;
        } else {
            const Term* ho_vars[64];
            for (size_t j = 0; j < rec->ho_telescope_count; j++) {
                ho_vars[j] = term_new_var(rec->ho_telescope_count - 1 - j,
                    rec->ho_telescope_names ? rec->ho_telescope_names[j] : NULL);
            }
            Term* field_applied = (Term*)field_value;
            for (size_t j = 0; j < rec->ho_telescope_count; j++) field_applied = term_new_app(field_applied, ho_vars[j]);
            
            const Term* body_args[256];
            size_t body_arg_count = 0;
            for (size_t j = 0; j < ind->param_count; j++) body_args[body_arg_count++] = params[j];
            body_args[body_arg_count++] = motive;
            for (size_t j = 0; j < branch_count; j++) body_args[body_arg_count++] = branches[j];
            for (size_t j = 0; j < target_arg_count; j++) body_args[body_arg_count++] = target_args[j];
            body_args[body_arg_count++] = field_applied;
            
            Term* body = term_new_global(elim->name);
            for (size_t j = 0; j < body_arg_count; j++) body = term_new_app(body, body_args[j]);
            
            Term* lam_body = body;
            for (size_t j = rec->ho_telescope_count; j > 0; j--) {
                size_t idx = j - 1;
                const char* ho_name = rec->ho_telescope_names ? rec->ho_telescope_names[idx] : NULL;
                const Term* ho_type = rec->ho_telescope_types ? rec->ho_telescope_types[idx] : term_new_type();
                const Term* inst_type = term_instantiate_env(ho_type, prefix_args, prefix_count, 0);
                lam_body = term_new_lam(ho_name, inst_type, lam_body);
            }
            ihs[ih_count++] = lam_body;
        }
    }
    
    Term* result = (Term*)branch;
    for (size_t i = 0; i < field_count; i++) result = term_new_app(result, field_args[i]);
    for (size_t i = 0; i < ih_count; i++) result = term_new_app(result, ihs[i]);
    for (size_t i = 0; i < extra_count; i++) result = term_new_app(result, extra[i]);
    
    app_chain_free(&scrut_chain);
    return result;
}

/* ============================================================
 * whnf 核心实现
 * ============================================================ */

static Term* _whnf(Reducer* r, const Term* term, bool allow_delta) {
    if (term == NULL) return NULL;
    
    const Term* current = term;
    uint32_t steps = 0;
    
    while (1) {
        if (steps >= r->max_steps) assert(0 && "WHNF exceeded step limit");
        steps++;
        
        AppChain chain = term_unfold_app(current);
        const Term* head = chain.head;
        const Term** args = chain.args;
        size_t arg_count = chain.count;
        
        Term* beta = try_beta(head, args, arg_count);
        if (beta != NULL) { app_chain_free(&chain); current = beta; continue; }
        
        if (head->kind == TERM_GLOBAL) {
            Term* iota = try_iota(r, head->global, args, arg_count);
            if (iota != NULL) { app_chain_free(&chain); current = iota; continue; }
        }
        
        if (head->kind == TERM_GLOBAL) {
            Term* delta = try_delta(r, head->global, args, arg_count, allow_delta);
            if (delta != NULL) { app_chain_free(&chain); current = delta; continue; }
        }
        
        if (current->kind == TERM_APP) {
            Term* fn_whnf = _whnf(r, current->app.func, allow_delta);
            if (fn_whnf != current->app.func) {
                current = term_new_app(fn_whnf, current->app.arg);
                app_chain_free(&chain);
                continue;
            }
        }
        
        app_chain_free(&chain);
        return (Term*)current;
    }
}

/* ============================================================
 * whnf - 弱头范式
 * ============================================================ */

Term* reducer_whnf(Reducer* r, const Term* term) {
    return _whnf(r, term, true);
}

Term* reducer_whnf_no_delta(Reducer* r, const Term* term) {
    return _whnf(r, term, false);
}

/* ============================================================
 * nf - 范式
 * ============================================================ */

Term* reducer_nf(Reducer* r, const Term* term) {
    Term* wh = reducer_whnf(r, term);
    
    switch (wh->kind) {
        case TERM_TYPE:
        case TERM_VAR:
        case TERM_GLOBAL:
            return wh;
        
        case TERM_PI: {
            Term* new_domain = reducer_nf(r, wh->pi.domain);
            Term* new_codomain = reducer_nf(r, wh->pi.codomain);
            if (new_domain == wh->pi.domain && new_codomain == wh->pi.codomain) {
                return wh;
            }
            return term_new_pi(wh->pi.name, new_domain, new_codomain);
        }
        
        case TERM_LAM: {
            Term* new_param_type = reducer_nf(r, wh->lam.param_type);
            Term* new_body = reducer_nf(r, wh->lam.body);
            if (new_param_type == wh->lam.param_type && new_body == wh->lam.body) {
                return wh;
            }
            return term_new_lam(wh->lam.name, new_param_type, new_body);
        }
        
        case TERM_APP: {
            Term* new_func = reducer_nf(r, wh->app.func);
            Term* new_arg = reducer_nf(r, wh->app.arg);
            Term* rebuilt = term_new_app(new_func, new_arg);
            Term* stepped = reducer_step(r, rebuilt);
            if (stepped == NULL) {
                return rebuilt;
            }
            return reducer_nf(r, stepped);
        }
    }
    
    assert(0 && "unreachable");
    return NULL;
}

/* ============================================================
 * 单步归约
 * ============================================================ */

Term* reducer_step(Reducer* r, const Term* term) {
    AppChain chain = term_unfold_app(term);
    const Term* head = chain.head;
    const Term** args = chain.args;
    size_t arg_count = chain.count;
    
    Term* result = NULL;
    
    result = try_beta(head, args, arg_count);
    if (result != NULL) {
        app_chain_free(&chain);
        return result;
    }
    
    if (head->kind == TERM_GLOBAL) {
        result = try_iota(r, head->global, args, arg_count);
        if (result != NULL) {
            app_chain_free(&chain);
            return result;
        }
        
        result = try_delta(r, head->global, args, arg_count, true);
        if (result != NULL) {
            app_chain_free(&chain);
            return result;
        }
    }
    
    app_chain_free(&chain);
    return NULL;
}

/* ============================================================
 * whnfv2 策略辅助函数
 * ============================================================ */

/* 收集可展开的全局名 */
static void collect_expandable_globals(Reducer* r, const Term* term, 
                                       char** names, uint32_t* count, uint32_t max) {
    if (term == NULL) return;
    
    switch (term->kind) {
        case TERM_GLOBAL: {
            GlobalEntry* entry = global_ctx_lookup(r->global_ctx, term->global);
            if (entry != NULL && entry->value != NULL) {
                /* 检查是否已存在 */
                for (uint32_t i = 0; i < *count; i++) {
                    if (strcmp(names[i], term->global) == 0) return;
                }
                if (*count < max) {
                    names[*count] = strdup(term->global);
                    (*count)++;
                }
            }
            return;
        }
        case TERM_TYPE:
        case TERM_VAR:
            return;
        case TERM_PI:
            collect_expandable_globals(r, term->pi.domain, names, count, max);
            collect_expandable_globals(r, term->pi.codomain, names, count, max);
            return;
        case TERM_LAM:
            collect_expandable_globals(r, term->lam.param_type, names, count, max);
            collect_expandable_globals(r, term->lam.body, names, count, max);
            return;
        case TERM_APP:
            collect_expandable_globals(r, term->app.func, names, count, max);
            collect_expandable_globals(r, term->app.arg, names, count, max);
            return;
    }
}

/* 计算全局名的依赖高度 */
static uint32_t global_height(Reducer* r, const char* name, 
                              char** cache_names, uint32_t* cache_heights, uint32_t* cache_count,
                              char** visiting, uint32_t* visiting_count) {
    /* 检查缓存 */
    for (uint32_t i = 0; i < *cache_count; i++) {
        if (strcmp(cache_names[i], name) == 0) {
            return cache_heights[i];
        }
    }
    
    /* 检查环 */
    for (uint32_t i = 0; i < *visiting_count; i++) {
        if (strcmp(visiting[i], name) == 0) {
            return r->max_steps;
        }
    }
    
    GlobalEntry* entry = global_ctx_lookup(r->global_ctx, name);
    if (entry == NULL || entry->value == NULL) {
        /* 缓存结果 */
        if (*cache_count < 1024) {
            cache_names[*cache_count] = strdup(name);
            cache_heights[*cache_count] = 0;
            (*cache_count)++;
        }
        return 0;
    }
    
    /* 标记为正在访问 */
    if (*visiting_count < 64) {
        visiting[*visiting_count] = strdup(name);
        (*visiting_count)++;
    }
    
    /* 收集引用 */
    char* refs[64];
    uint32_t ref_count = 0;
    collect_expandable_globals(r, entry->value, refs, &ref_count, 64);
    
    uint32_t height;
    if (ref_count == 0) {
        height = 1;
    } else {
        height = 0;
        for (uint32_t i = 0; i < ref_count; i++) {
            uint32_t h = global_height(r, refs[i], cache_names, cache_heights, cache_count,
                                       visiting, visiting_count);
            if (h > height) height = h;
        }
        height++;
    }
    
    /* 清理引用 */
    for (uint32_t i = 0; i < ref_count; i++) {
        free(refs[i]);
    }
    
    /* 移除正在访问标记 */
    if (*visiting_count > 0) {
        (*visiting_count)--;
    }
    
    /* 缓存结果 */
    if (*cache_count < 1024) {
        cache_names[*cache_count] = strdup(name);
        cache_heights[*cache_count] = height;
        (*cache_count)++;
    }
    
    return height;
}

/* 选择最优的 Delta 展开目标 */
static const char* pick_delta_name(Reducer* r, const Term* lhs, const Term* rhs) {
    char* lhs_names[64];
    uint32_t lhs_count = 0;
    char* rhs_names[64];
    uint32_t rhs_count = 0;
    
    collect_expandable_globals(r, lhs, lhs_names, &lhs_count, 64);
    collect_expandable_globals(r, rhs, rhs_names, &rhs_count, 64);
    
    /* 合并候选 */
    char* candidates[128];
    uint32_t candidate_count = 0;
    
    for (uint32_t i = 0; i < lhs_count; i++) {
        candidates[candidate_count++] = lhs_names[i];
    }
    for (uint32_t i = 0; i < rhs_count; i++) {
        /* 检查是否已存在 */
        bool exists = false;
        for (uint32_t j = 0; j < candidate_count; j++) {
            if (strcmp(candidates[j], rhs_names[i]) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            candidates[candidate_count++] = rhs_names[i];
        } else {
            free(rhs_names[i]);
        }
    }
    
    if (candidate_count == 0) {
        return NULL;
    }
    
    /* 计算高度并选择最小的 */
    char* cache_names[1024];
    uint32_t cache_heights[1024];
    uint32_t cache_count = 0;
    char* visiting[64];
    uint32_t visiting_count = 0;
    
    const char* best = NULL;
    uint32_t best_height = UINT32_MAX;
    
    for (uint32_t i = 0; i < candidate_count; i++) {
        uint32_t h = global_height(r, candidates[i], cache_names, cache_heights, &cache_count,
                                   visiting, &visiting_count);
        if (h < best_height || (h == best_height && (best == NULL || strcmp(candidates[i], best) < 0))) {
            best_height = h;
            best = candidates[i];
        }
    }
    
    /* 清理 */
    for (uint32_t i = 0; i < candidate_count; i++) {
        if (candidates[i] != best) {
            free(candidates[i]);
        }
    }
    for (uint32_t i = 0; i < cache_count; i++) {
        free(cache_names[i]);
    }
    
    return best;
}

/* 展开指定全局名 */
static Term* expand_named_global(Reducer* r, const Term* term, const char* target_name) {
    if (term == NULL) return NULL;
    
    switch (term->kind) {
        case TERM_GLOBAL: {
            if (strcmp(term->global, target_name) != 0) return (Term*)term;
            GlobalEntry* entry = global_ctx_lookup(r->global_ctx, target_name);
            if (entry == NULL || entry->value == NULL) return (Term*)term;
            return (Term*)entry->value;
        }
        case TERM_TYPE:
        case TERM_VAR:
            return (Term*)term;
        case TERM_PI: {
            Term* domain = expand_named_global(r, term->pi.domain, target_name);
            Term* codomain = expand_named_global(r, term->pi.codomain, target_name);
            if (domain == term->pi.domain && codomain == term->pi.codomain) return (Term*)term;
            return term_new_pi(term->pi.name, domain, codomain);
        }
        case TERM_LAM: {
            Term* param_type = expand_named_global(r, term->lam.param_type, target_name);
            Term* body = expand_named_global(r, term->lam.body, target_name);
            if (param_type == term->lam.param_type && body == term->lam.body) return (Term*)term;
            return term_new_lam(term->lam.name, param_type, body);
        }
        case TERM_APP: {
            Term* func = expand_named_global(r, term->app.func, target_name);
            Term* arg = expand_named_global(r, term->app.arg, target_name);
            if (func == term->app.func && arg == term->app.arg) return (Term*)term;
            return term_new_app(func, arg);
        }
    }
    
    return (Term*)term;
}

/* ============================================================
 * is_def_eq - 定义相等检查
 * ============================================================ */

static bool struct_eq(const Term* a, const Term* b) {
    if (a == b) return true;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TERM_VAR:
            return a->var.index == b->var.index;
        case TERM_GLOBAL:
            return strcmp(a->global, b->global) == 0;
        case TERM_TYPE:
            return true;
        case TERM_PI:
            return struct_eq(a->pi.domain, b->pi.domain) &&
                   struct_eq(a->pi.codomain, b->pi.codomain);
        case TERM_LAM:
            return struct_eq(a->lam.param_type, b->lam.param_type) &&
                   struct_eq(a->lam.body, b->lam.body);
        case TERM_APP:
            return struct_eq(a->app.func, b->app.func) &&
                   struct_eq(a->app.arg, b->app.arg);
    }
    
    return false;
}

/* whnf + 结构比较 */
static bool _whnf_compare(Reducer* r, const Term* lhs, const Term* rhs, bool allow_delta) {
    Term* a = allow_delta ? reducer_whnf(r, lhs) : reducer_whnf_no_delta(r, lhs);
    Term* b = allow_delta ? reducer_whnf(r, rhs) : reducer_whnf_no_delta(r, rhs);
    
    /* 空指针检查 */
    if (a == NULL || b == NULL) {
        return a == b;  /* 都为 NULL 则相等 */
    }
    
    if (a == b) return true;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TERM_TYPE:
            return true;
        case TERM_VAR:
            return a->var.index == b->var.index;
        case TERM_GLOBAL:
            return strcmp(a->global, b->global) == 0;
        case TERM_PI:
            return _whnf_compare(r, a->pi.domain, b->pi.domain, allow_delta) &&
                   _whnf_compare(r, a->pi.codomain, b->pi.codomain, allow_delta);
        case TERM_LAM:
            return _whnf_compare(r, a->lam.param_type, b->lam.param_type, allow_delta) &&
                   _whnf_compare(r, a->lam.body, b->lam.body, allow_delta);
        case TERM_APP: {
            /* 展开应用链进行比较 */
            AppChain chain_a = term_unfold_app(a);
            AppChain chain_b = term_unfold_app(b);
            
            if (chain_a.count != chain_b.count) {
                app_chain_free(&chain_a);
                app_chain_free(&chain_b);
                return false;
            }
            
            bool result = _whnf_compare(r, chain_a.head, chain_b.head, allow_delta);
            for (size_t i = 0; result && i < chain_a.count; i++) {
                result = _whnf_compare(r, chain_a.args[i], chain_b.args[i], allow_delta);
            }
            
            app_chain_free(&chain_a);
            app_chain_free(&chain_b);
            return result;
        }
    }
    
    return false;
}

/* whnfv2 策略 */
static bool _is_def_eq_whnfv2(Reducer* r, const Term* lhs, const Term* rhs) {
    const Term* current_lhs = lhs;
    const Term* current_rhs = rhs;
    uint32_t steps = 0;
    
    while (1) {
        steps++;
        if (steps > r->max_steps) return false;
        
        /* 快速语法检查 */
        if (current_lhs == current_rhs) return true;
        
        /* Beta + Iota WHNF 结构比较（无 Delta） */
        if (_whnf_compare(r, current_lhs, current_rhs, false)) return true;
        
        /* 选择最优的 Delta 展开目标 */
        const char* choice = pick_delta_name(r, current_lhs, current_rhs);
        if (choice == NULL) return false;
        
        /* 展开选择的全局名 */
        Term* next_lhs = expand_named_global(r, current_lhs, choice);
        Term* next_rhs = expand_named_global(r, current_rhs, choice);
        
        if (next_lhs == current_lhs && next_rhs == current_rhs) return false;
        
        current_lhs = next_lhs;
        current_rhs = next_rhs;
    }
}

/* 获取全局定义的值 */
static const Term* global_value(Reducer* r, const char* name) {
    GlobalEntry* entry = global_ctx_lookup(r->global_ctx, name);
    if (entry != NULL && entry->value != NULL) {
        return entry->value;
    }
    return NULL;
}

/* greedy 策略 */
static bool _is_def_eq_greedy(Reducer* r, const Term* lhs, const Term* rhs, uint32_t steps) {
    if (steps > r->max_steps) return false;
    if (lhs == rhs) return true;
    if (struct_eq(lhs, rhs)) return true;
    
    /* CType vs CType */
    if (lhs->kind == TERM_TYPE && rhs->kind == TERM_TYPE) return true;
    
    /* CVar vs CVar */
    if (lhs->kind == TERM_VAR && rhs->kind == TERM_VAR) {
        return lhs->var.index == rhs->var.index;
    }
    
    /* CGlobal vs CGlobal */
    if (lhs->kind == TERM_GLOBAL && rhs->kind == TERM_GLOBAL) {
        if (strcmp(lhs->global, rhs->global) == 0) return true;
        const Term* lv = global_value(r, lhs->global);
        const Term* rv = global_value(r, rhs->global);
        if (lv != NULL && rv != NULL) return _is_def_eq_greedy(r, lv, rv, steps + 1);
        if (lv != NULL) return _is_def_eq_greedy(r, lv, rhs, steps + 1);
        if (rv != NULL) return _is_def_eq_greedy(r, lhs, rv, steps + 1);
        return false;
    }
    
    /* CPi vs CPi */
    if (lhs->kind == TERM_PI && rhs->kind == TERM_PI) {
        return _is_def_eq_greedy(r, lhs->pi.domain, rhs->pi.domain, steps + 1) &&
               _is_def_eq_greedy(r, lhs->pi.codomain, rhs->pi.codomain, steps + 1);
    }
    
    /* CLam vs CLam */
    if (lhs->kind == TERM_LAM && rhs->kind == TERM_LAM) {
        return _is_def_eq_greedy(r, lhs->lam.param_type, rhs->lam.param_type, steps + 1) &&
               _is_def_eq_greedy(r, lhs->lam.body, rhs->lam.body, steps + 1);
    }
    
    /* CApp vs CApp */
    if (lhs->kind == TERM_APP && rhs->kind == TERM_APP) {
        if (_is_def_eq_greedy(r, lhs->app.func, rhs->app.func, steps + 1) &&
            _is_def_eq_greedy(r, lhs->app.arg, rhs->app.arg, steps + 1)) {
            return true;
        }
        Term* ls = reducer_step(r, lhs);
        Term* rs = reducer_step(r, rhs);
        if (ls == NULL && rs == NULL) return false;
        return _is_def_eq_greedy(r, ls ? ls : lhs, rs ? rs : rhs, steps + 1);
    }
    
    /* 跨类型比较 */
    if (lhs->kind == TERM_TYPE && rhs->kind == TERM_GLOBAL) {
        const Term* rv = global_value(r, rhs->global);
        if (rv != NULL) return _is_def_eq_greedy(r, lhs, rv, steps + 1);
        return false;
    }
    if (lhs->kind == TERM_GLOBAL && rhs->kind == TERM_TYPE) {
        const Term* lv = global_value(r, lhs->global);
        if (lv != NULL) return _is_def_eq_greedy(r, lv, rhs, steps + 1);
        return false;
    }
    
    /* 其他情况尝试归约 */
    if (lhs->kind == TERM_APP || rhs->kind == TERM_APP) {
        Term* ls = (lhs->kind == TERM_APP) ? reducer_step(r, lhs) : NULL;
        Term* rs = (rhs->kind == TERM_APP) ? reducer_step(r, rhs) : NULL;
        if (ls != NULL || rs != NULL) {
            return _is_def_eq_greedy(r, ls ? ls : lhs, rs ? rs : rhs, steps + 1);
        }
    }
    
    if (lhs->kind == TERM_GLOBAL) {
        const Term* lv = global_value(r, lhs->global);
        if (lv != NULL) return _is_def_eq_greedy(r, lv, rhs, steps + 1);
    }
    if (rhs->kind == TERM_GLOBAL) {
        const Term* rv = global_value(r, rhs->global);
        if (rv != NULL) return _is_def_eq_greedy(r, lhs, rv, steps + 1);
    }
    
    return false;
}

bool reducer_is_def_eq(Reducer* r, const Term* lhs, const Term* rhs) {
    if (lhs == rhs) return true;
    
    switch (r->strategy) {
        case CONV_GREEDY:
            return _is_def_eq_greedy(r, lhs, rhs, 0);
        case CONV_WHN:
            return _whnf_compare(r, lhs, rhs, true);
        case CONV_WHNFv2:
            return _is_def_eq_whnfv2(r, lhs, rhs);
    }
    
    return false;
}
