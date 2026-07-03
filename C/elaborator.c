#include "elaborator.h"
#include "restricted.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * ElabLocalContext
 * ============================================================ */

void elab_local_init(ElabLocalContext* ctx) {
    ctx->names = NULL;
    ctx->count = 0;
}

void elab_local_free(ElabLocalContext* ctx) {
    free(ctx->names);
    ctx->names = NULL;
    ctx->count = 0;
}

void elab_local_extend(ElabLocalContext* ctx, const char* name) {
    ctx->names = (char**)realloc(ctx->names, (ctx->count + 1) * sizeof(char*));
    ctx->names[ctx->count] = (char*)name;
    ctx->count++;
}

/* ============================================================
 * ElabState
 * ============================================================ */

void elab_state_init(ElabState* state) {
    state->tag_count = 0;
    state->inductive_count = 0;
    state->example_counter = 0;
    state->surface_counter = 0;
    state->fresh_count = 0;
}

void elab_state_free(ElabState* state) {
    for (uint32_t i = 0; i < state->tag_count; i++) {
        free(state->tag_names[i]);
        free(state->tag_values[i]);
    }
    for (uint32_t i = 0; i < state->inductive_count; i++) {
        /* TODO: 释放 InductiveSpec */
    }
    state->tag_count = 0;
    state->inductive_count = 0;
}

const char* elab_state_fresh(ElabState* state, const char* prefix) {
    const char* stem = prefix ? prefix : "x";
    
    /* 检查是否是 singleton prefix */
    if (strcmp(stem, "motive") == 0 || strcmp(stem, "q") == 0 || strcmp(stem, "d") == 0) {
        /* 直接返回 _stem */
        char* result = (char*)malloc(strlen(stem) + 2);
        sprintf(result, "_%s", stem);
        return result;
    }
    
    /* 查找或创建计数器 */
    uint32_t counter_index = 0;
    for (uint32_t i = 0; i < state->fresh_count; i++) {
        if (strcmp(state->fresh_prefixes[i], stem) == 0) {
            counter_index = i;
            break;
        }
    }
    if (counter_index == 0 && state->fresh_count == 0) {
        state->fresh_prefixes[state->fresh_count] = strdup(stem);
        state->fresh_counts[state->fresh_count] = 0;
        counter_index = state->fresh_count;
        state->fresh_count++;
    }
    
    uint32_t c = state->fresh_counts[counter_index];
    while (1) {
        char* name;
        if (c == 0) {
            name = (char*)malloc(strlen(stem) + 2);
            sprintf(name, "_%s", stem);
        } else {
            name = (char*)malloc(strlen(stem) + 16);
            sprintf(name, "_%s%u", stem, c);
        }
        c++;
        
        /* 检查是否已存在 */
        bool exists = false;
        for (uint32_t i = 0; i < state->tag_count; i++) {
            if (strcmp(state->tag_names[i], name) == 0) {
                exists = true;
                free(name);
                break;
            }
        }
        if (!exists) {
            state->fresh_counts[counter_index] = c;
            return name;
        }
    }
}

const char* elab_state_fresh_example_name(ElabState* state) {
    while (1) {
        state->example_counter++;
        char* name = (char*)malloc(32);
        sprintf(name, "example_%u", state->example_counter);
        
        bool exists = false;
        for (uint32_t i = 0; i < state->tag_count; i++) {
            if (strcmp(state->tag_names[i], name) == 0) {
                exists = true;
                free(name);
                break;
            }
        }
        if (!exists) {
            return name;
        }
    }
}

void elab_state_register_global(ElabState* state, const char* name, const char* tag) {
    if (state->tag_count >= MAX_GLOBAL_TAGS) return;
    state->tag_names[state->tag_count] = strdup(name);
    state->tag_values[state->tag_count] = strdup(tag);
    state->tag_count++;
}

bool elab_state_is_defined(ElabState* state, const char* name) {
    for (uint32_t i = 0; i < state->tag_count; i++) {
        if (strcmp(state->tag_names[i], name) == 0) return true;
    }
    return false;
}

const char* elab_state_resolve_dot(ElabState* state, const char* text) {
    if (strchr(text, '.') == NULL) {
        return elab_state_is_defined(state, text) ? text : NULL;
    }
    
    /* 分割 module.name */
    char* dot = strchr(text, '.');
    size_t fst_len = dot - text;
    char* fst = (char*)malloc(fst_len + 1);
    strncpy(fst, text, fst_len);
    fst[fst_len] = '\0';
    const char* snd = dot + 1;
    
    /* 查找归纳类型 */
    for (uint32_t i = 0; i < state->inductive_count; i++) {
        if (strcmp(state->inductives[i]->name, fst) == 0) {
            InductiveSpec* spec = state->inductives[i];
            if (strcmp(spec->kind, "inductive") == 0 && strcmp(snd, "rec") == 0) {
                free(fst);
                return spec->eliminator_name;
            }
            if ((strcmp(spec->kind, "sum") == 0 || strcmp(spec->kind, "product") == 0) && 
                strcmp(snd, "elim") == 0) {
                free(fst);
                return spec->eliminator_name;
            }
            if (strcmp(spec->kind, "product") == 0 && strcmp(snd, "mk") == 0) {
                free(fst);
                return spec->ctor_names[0];
            }
            free(fst);
            return NULL;
        }
    }
    
    free(fst);
    return elab_state_is_defined(state, text) ? text : NULL;
}

/* 辅助函数：深拷贝 Expr */
static Expr* expr_deep_copy(Expr* expr) {
    if (expr == NULL) return NULL;
    switch (expr->kind) {
        case EXPR_ATOM:
            return expr_new_atom(expr->atom.text ? strdup(expr->atom.text) : NULL);
        case EXPR_APP:
            return expr_new_app(expr_deep_copy(expr->app.func), expr_deep_copy(expr->app.arg));
        case EXPR_PI:
            return expr_new_pi(expr->pi.name ? strdup(expr->pi.name) : NULL,
                              expr_deep_copy(expr->pi.domain),
                              expr_deep_copy(expr->pi.codomain));
        case EXPR_LAMBDA: {
            Param* p = param_new(
                expr->lambda.param->name ? strdup(expr->lambda.param->name) : NULL,
                expr_deep_copy(expr->lambda.param->typ));
            return expr_new_lambda(p, expr_deep_copy(expr->lambda.body));
        }
        default:
            return expr;
    }
}

/* 辅助函数：构建应用表达式 */
static Expr* mk_apps(Expr* head, Expr** args, uint32_t count) {
    Expr* result = head;
    for (uint32_t i = 0; i < count; i++) {
        result = expr_new_app(result, args[i]);
    }
    return result;
}

/* 辅助函数：shift Expr 中的自由变量索引 */
static Expr* term_shift_expr(Expr* expr, uint32_t amount) {
    if (expr == NULL || amount == 0) return expr;
    switch (expr->kind) {
        case EXPR_ATOM:
            return expr;  /* 原子不包含 de Bruijn 索引 */
        case EXPR_APP: {
            Expr* new_func = term_shift_expr(expr->app.func, amount);
            Expr* new_arg = term_shift_expr(expr->app.arg, amount);
            if (new_func == expr->app.func && new_arg == expr->app.arg) return expr;
            return expr_new_app(new_func, new_arg);
        }
        case EXPR_PI: {
            Expr* new_domain = term_shift_expr(expr->pi.domain, amount);
            Expr* new_codomain = term_shift_expr(expr->pi.codomain, amount);
            if (new_domain == expr->pi.domain && new_codomain == expr->pi.codomain) return expr;
            return expr_new_pi(expr->pi.name, new_domain, new_codomain);
        }
        case EXPR_LAMBDA: {
            Expr* new_param_type = term_shift_expr(expr->lambda.param->typ, amount);
            Expr* new_body = term_shift_expr(expr->lambda.body, amount);
            if (new_param_type == expr->lambda.param->typ && new_body == expr->lambda.body) return expr;
            Param* p = param_new(expr->lambda.param->name, new_param_type);
            return expr_new_lambda(p, new_body);
        }
        default:
            return expr;
    }
}

/* 辅助函数：构建原子表达式列表 */
static Expr** mk_atoms(const char** names, uint32_t count) {
    Expr** atoms = (Expr**)malloc(count * sizeof(Expr*));
    for (uint32_t i = 0; i < count; i++) {
        atoms[i] = expr_new_atom(strdup(names[i]));
    }
    return atoms;
}

/* 辅助函数：检查表达式是否包含特定原子 */
static bool expr_contains_atom(Expr* expr, const char* name) {
    if (expr == NULL) return false;
    if (expr->kind == EXPR_ATOM) {
        return expr->atom.text != NULL && strcmp(expr->atom.text, name) == 0;
    }
    if (expr->kind == EXPR_APP) {
        return expr_contains_atom(expr->app.func, name) || expr_contains_atom(expr->app.arg, name);
    }
    if (expr->kind == EXPR_PI) {
        return expr_contains_atom(expr->pi.domain, name) || expr_contains_atom(expr->pi.codomain, name);
    }
    if (expr->kind == EXPR_LAMBDA) {
        return expr_contains_atom(expr->lambda.param->typ, name) || expr_contains_atom(expr->lambda.body, name);
    }
    return false;
}

/* compile_recursive_fields - 递归字段分析 */
static ElabRecFieldInfo* compile_recursive_fields(
    const char* decl_kind,
    const char* owner,
    const char** param_names,
    uint32_t param_count,
    uint32_t index_count,
    Expr** field_types,
    uint32_t field_count,
    uint32_t* out_count
) {
    ElabRecFieldInfo* result = NULL;
    uint32_t count = 0;
    uint32_t family_arity = param_count + index_count;
    
    for (uint32_t i = 0; i < field_count; i++) {
        Expr* field_type = field_types[i];
        
        /* 检查 sum/product 的字段不能提到 owner */
        if ((strcmp(decl_kind, "sum") == 0 || strcmp(decl_kind, "product") == 0) && 
            expr_contains_atom(field_type, owner)) {
            continue;  /* 跳过，不报错 */
        }
        
        /* 拆解 Pi 类型 */
        PiTelescope tel = split_pi_expr(field_type);
        
        /* 处理 family_arity == 0 的情况（如 Nat.succ 的参数类型就是 Nat 本身） */
        if (family_arity == 0) {
            if (tel.count == 0 && tel.body != NULL && tel.body->kind == EXPR_ATOM &&
                strcmp(tel.body->atom.text, owner) == 0) {
                /* 直接递归字段，无参数 */
                result = (ElabRecFieldInfo*)realloc(result, (count + 1) * sizeof(ElabRecFieldInfo));
                result[count].field_position = i;
                result[count].recursive_kind = strdup("direct");
                result[count].ho_names = NULL;
                result[count].ho_types = NULL;
                result[count].ho_count = 0;
                result[count].target_index_count = 0;
                result[count].target_index_exprs = NULL;
                count++;
            }
            pi_telescope_free(&tel);
            continue;
        }
        
        /* 检查结果类型是否是 owner */
        if (tel.body == NULL || tel.body->kind != EXPR_APP) {
            pi_telescope_free(&tel);
            continue;
        }
        
        /* 展开应用链 */
        Expr* head = tel.body;
        Expr* args[64];
        uint32_t arg_count = 0;
        while (head->kind == EXPR_APP && arg_count < 64) {
            args[arg_count++] = head->app.arg;
            head = head->app.func;
        }
        
        if (head->kind != EXPR_ATOM || strcmp(head->atom.text, owner) != 0) {
            pi_telescope_free(&tel);
            continue;
        }
        
        if (arg_count != family_arity) {
            pi_telescope_free(&tel);
            continue;
        }
        
        /* 检查参数是否一致 */
        bool uniform = true;
        for (uint32_t j = 0; j < param_count; j++) {
            if (args[j]->kind != EXPR_ATOM || strcmp(args[j]->atom.text, param_names[j]) != 0) {
                uniform = false;
                break;
            }
        }
        if (!uniform) {
            pi_telescope_free(&tel);
            continue;
        }
        
        /* 添加到结果 */
        result = (ElabRecFieldInfo*)realloc(result, (count + 1) * sizeof(ElabRecFieldInfo));
        result[count].field_position = i;
        result[count].recursive_kind = strdup(tel.count > 0 ? "higher" : "direct");
        result[count].ho_names = tel.names;
        result[count].ho_types = tel.types;
        result[count].ho_count = tel.count;
        result[count].target_index_count = index_count;
        result[count].target_index_exprs = (Expr**)malloc(index_count * sizeof(Expr*));
        for (uint32_t j = 0; j < index_count; j++) {
            result[count].target_index_exprs[j] = args[param_count + j];
        }
        count++;
    }
    
    *out_count = count;
    return result;
}

/* ============================================================
 * Elaborator
 * ============================================================ */

void elaborator_init(Elaborator* elab) {
    elab_state_init(&elab->state);
}

void elaborator_free(Elaborator* elab) {
    elab_state_free(&elab->state);
}

/* 辅助函数：创建原子表达式 */
static Expr* mk_atom(Elaborator* elab, const char* text) {
    return expr_new_atom(strdup(text));
}

/* 辅助函数：构建应用表达式 */
static Expr* elab_mk_apps(Expr* head, Expr** args, uint32_t count) {
    Expr* result = head;
    for (uint32_t i = 0; i < count; i++) {
        result = expr_new_app(result, args[i]);
    }
    return result;
}

/* ============================================================
 * instantiate_telescope - 实例化 telescope
 * ============================================================ */

static void instantiate_telescope(
    char** orig_names, Expr** orig_types, uint32_t count,
    char** actuals,
    char*** out_names, Expr*** out_types
) {
    *out_names = (char**)malloc(count * sizeof(char*));
    *out_types = (Expr**)malloc(count * sizeof(Expr*));
    
    /* 构建替换映射 */
    const char** mapping_keys = (const char**)malloc(count * sizeof(const char*));
    Expr** mapping_values = (Expr**)malloc(count * sizeof(Expr*));
    uint32_t mapping_count = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        /* 替换后的类型 */
        (*out_names)[i] = strdup(actuals[i]);
        (*out_types)[i] = substitute_expr(
            orig_types[i], mapping_keys, mapping_values, mapping_count,
            NULL, NULL
        );
        
        /* 添加到映射 */
        mapping_keys[mapping_count] = orig_names[i];
        mapping_values[mapping_count] = expr_new_atom(strdup(actuals[i]));
        mapping_count++;
    }
    
    free(mapping_keys);
    free(mapping_values);
}

/* ============================================================
 * build_instantiated_ih_type - 构建实例化的 IH 类型
 * ============================================================ */

static Expr* build_instantiated_ih_type(
    InductiveSpec* spec,
    ElabCtorInfo* ctor,
    ElabRecFieldInfo* rec,
    Expr** param_terms,
    char** field_actuals,
    Expr* motive_expr
) {
    /* 构建 param_mapping */
    uint32_t max_mapping = spec->param_count + ctor->field_count;
    const char** mapping_keys = (const char**)malloc(max_mapping * sizeof(const char*));
    Expr** mapping_values = (Expr**)malloc(max_mapping * sizeof(Expr*));
    uint32_t mapping_count = 0;
    
    for (uint32_t i = 0; i < spec->param_count; i++) {
        mapping_keys[mapping_count] = spec->param_names[i];
        mapping_values[mapping_count] = param_terms[i];
        mapping_count++;
    }
    
    /* 添加前面的字段到映射 */
    for (uint32_t i = 0; i < rec->field_position && i < ctor->field_count; i++) {
        mapping_keys[mapping_count] = ctor->field_names[i];
        mapping_values[mapping_count] = expr_new_atom(strdup(field_actuals[i]));
        mapping_count++;
    }
    
    char* field_actual = field_actuals[rec->field_position];
    
    if (strcmp(rec->recursive_kind, "direct") == 0) {
        /* 直接递归：motive p1 ... i1 ... field_name */
        Expr** args = (Expr**)malloc((spec->param_count + spec->index_count + 1) * sizeof(Expr*));
        uint32_t arg_count = 0;
        
        for (uint32_t i = 0; i < spec->param_count; i++) {
            args[arg_count++] = param_terms[i];
        }
        
        /* 实例化 target_index_exprs */
        for (uint32_t i = 0; i < rec->target_index_count; i++) {
            args[arg_count++] = substitute_expr(
                rec->target_index_exprs[i], mapping_keys, mapping_values, mapping_count,
                NULL, NULL
            );
        }
        
        args[arg_count++] = expr_new_atom(strdup(field_actual));
        
        Expr* result = elab_mk_apps(motive_expr, args, arg_count);
        
        free(mapping_keys);
        free(mapping_values);
        free(args);
        
        return result;
    }
    
    /* 高阶递归 */
    /* 构建 ho_bindings 和 ho_mapping */
    uint32_t ho_count = rec->ho_count;
    char** ho_names_arr = (char**)malloc(ho_count * sizeof(char*));
    Expr** ho_types_arr = (Expr**)malloc(ho_count * sizeof(Expr*));
    
    /* 复制 mapping 为 ho_mapping */
    uint32_t ho_mapping_cap = mapping_count + ho_count;
    const char** ho_mapping_keys = (const char**)malloc(ho_mapping_cap * sizeof(const char*));
    Expr** ho_mapping_values = (Expr**)malloc(ho_mapping_cap * sizeof(Expr*));
    uint32_t ho_mapping_count = 0;
    for (uint32_t i = 0; i < mapping_count; i++) {
        ho_mapping_keys[ho_mapping_count] = mapping_keys[i];
        ho_mapping_values[ho_mapping_count] = mapping_values[i];
        ho_mapping_count++;
    }
    
    char** ho_actuals = (char**)malloc(ho_count * sizeof(char*));
    
    for (uint32_t i = 0; i < ho_count; i++) {
        char* actual = strdup(rec->ho_names[i] ? rec->ho_names[i] : "_");
        ho_names_arr[i] = strdup(actual);
        ho_actuals[i] = actual;
        
        ho_types_arr[i] = substitute_expr(
            rec->ho_types[i], ho_mapping_keys, ho_mapping_values, ho_mapping_count,
            NULL, NULL
        );
        
        ho_mapping_keys[ho_mapping_count] = rec->ho_names[i];
        ho_mapping_values[ho_mapping_count] = expr_new_atom(strdup(actual));
        ho_mapping_count++;
    }
    
    /* 实例化 target_index_exprs */
    Expr** target_indices = (Expr**)malloc(rec->target_index_count * sizeof(Expr*));
    for (uint32_t i = 0; i < rec->target_index_count; i++) {
        target_indices[i] = substitute_expr(
            rec->target_index_exprs[i], ho_mapping_keys, ho_mapping_values, ho_mapping_count,
            NULL, NULL
        );
    }
    
    /* 构建 field_applied: field_actual ho_actuals[0] ho_actuals[1] ... */
    Expr* field_applied = expr_new_atom(strdup(field_actual));
    for (uint32_t i = 0; i < ho_count; i++) {
        field_applied = expr_new_app(field_applied, expr_new_atom(strdup(ho_actuals[i])));
    }
    
    /* 构建 body: motive p1 ... i1 ... target_indices ... field_applied */
    uint32_t body_arg_count = spec->param_count + spec->index_count + rec->target_index_count + 1;
    Expr** body_args = (Expr**)malloc(body_arg_count * sizeof(Expr*));
    uint32_t body_idx = 0;
    for (uint32_t i = 0; i < spec->param_count; i++) {
        body_args[body_idx++] = param_terms[i];
    }
    for (uint32_t i = 0; i < spec->index_count; i++) {
        body_args[body_idx++] = param_terms[spec->param_count + i];
    }
    for (uint32_t i = 0; i < rec->target_index_count; i++) {
        body_args[body_idx++] = target_indices[i];
    }
    body_args[body_idx++] = field_applied;
    
    Expr* body = elab_mk_apps(motive_expr, body_args, body_idx);
    
    Expr* result = fold_pi_expr(ho_names_arr, ho_types_arr, ho_count, body);
    
    free(mapping_keys);
    free(mapping_values);
    free(ho_mapping_keys);
    free(ho_mapping_values);
    free(ho_names_arr);
    free(ho_types_arr);
    free(ho_actuals);
    free(target_indices);
    free(body_args);
    
    return result;
}

/* ============================================================
 * build_match_branch - 构建 match 分支
 * ============================================================ */

static Expr* build_match_branch(
    Elaborator* elab,
    InductiveSpec* spec,
    ElabCtorInfo* ctor,
    MatchBranch* branch,
    ElabLocalContext* outer_local,
    Expr** param_terms,
    char** param_actuals,
    Expr* motive_expr,
    bool allow_ih
) {
    uint32_t field_count = ctor->field_count;
    uint32_t rec_count = ctor->recursive_field_count;
    
    /* 构建字段实际名和类型 */
    char** field_actuals = (char**)malloc(field_count * sizeof(char*));
    char** field_names_arr = (char**)malloc(field_count * sizeof(char*));
    Expr** field_types_arr = (Expr**)malloc(field_count * sizeof(Expr*));
    
    /* 构建 param_mapping */
    const char** param_mapping_keys = (const char**)malloc(spec->param_count * sizeof(const char*));
    Expr** param_mapping_values = (Expr**)malloc(spec->param_count * sizeof(Expr*));
    uint32_t param_mapping_count = 0;
    
    for (uint32_t i = 0; i < spec->param_count; i++) {
        param_mapping_keys[i] = spec->param_names[i];
        param_mapping_values[i] = param_terms[i];
        param_mapping_count++;
    }
    
    /* 处理字段 */
    const char** field_mapping_keys = (const char**)malloc((field_count + spec->param_count) * sizeof(const char*));
    Expr** field_mapping_values = (Expr**)malloc((field_count + spec->param_count) * sizeof(Expr*));
    uint32_t field_mapping_count = 0;
    
    /* 复制 param_mapping */
    for (uint32_t i = 0; i < param_mapping_count; i++) {
        field_mapping_keys[field_mapping_count] = param_mapping_keys[i];
        field_mapping_values[field_mapping_count] = param_mapping_values[i];
        field_mapping_count++;
    }
    
    for (uint32_t i = 0; i < field_count; i++) {
        char* raw_name = branch->fields[i];
        char* actual = raw_name ? strdup(raw_name) : strdup("_");
        field_actuals[i] = actual;
        field_names_arr[i] = strdup(actual);
        
        /* 替换字段类型 */
        field_types_arr[i] = substitute_expr(
            ctor->field_types[i], field_mapping_keys, field_mapping_values, field_mapping_count,
            NULL, NULL
        );
        
        /* 添加到映射 */
        field_mapping_keys[field_mapping_count] = ctor->field_names[i];
        field_mapping_values[field_mapping_count] = expr_new_atom(strdup(actual));
        field_mapping_count++;
    }
    
    /* 处理 IH */
    uint32_t ih_count = allow_ih ? rec_count : 0;
    char** ih_names = (char**)malloc(ih_count * sizeof(char*));
    Expr** ih_types = (Expr**)malloc(ih_count * sizeof(Expr*));
    
    for (uint32_t i = 0; i < ih_count; i++) {
        char* raw_ih = (branch->ih_count > i) ? branch->ihs[i] : NULL;
        ih_names[i] = raw_ih ? strdup(raw_ih) : strdup("_");
        ih_types[i] = build_instantiated_ih_type(
            spec, ctor, &ctor->recursive_fields[i],
            param_terms, field_actuals, motive_expr
        );
    }
    
    /* 构建分支上下文（与 Python 一致：outer + param_actuals + field_actuals + ih_names） */
    ElabLocalContext branch_local;
    elab_local_init(&branch_local);
    for (uint32_t i = 0; i < outer_local->count; i++) {
        elab_local_extend(&branch_local, outer_local->names[i]);
    }
    for (uint32_t i = 0; i < spec->param_count; i++) {
        elab_local_extend(&branch_local, param_actuals[i]);
    }
    for (uint32_t i = 0; i < field_count; i++) {
        elab_local_extend(&branch_local, field_actuals[i]);
    }
    for (uint32_t i = 0; i < ih_count; i++) {
        elab_local_extend(&branch_local, ih_names[i]);
    }
    
    /* 精细化分支体 */
    Expr* lowered_body = elab_lower_expr(elab, branch->body, &branch_local);
    
    /* 替换参数别名 */
    const char** alias_keys = (const char**)malloc(spec->param_count * sizeof(const char*));
    Expr** alias_values = (Expr**)malloc(spec->param_count * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->param_count; i++) {
        alias_keys[i] = param_actuals[i];
        alias_values[i] = param_terms[i];
    }
    lowered_body = substitute_expr(lowered_body, alias_keys, alias_values, spec->param_count, NULL, NULL);
    free(alias_keys);
    free(alias_values);
    
    /* 封装为 Lambda */
    uint32_t lam_count = field_count + ih_count;
    char** lam_names = (char**)malloc(lam_count * sizeof(char*));
    Expr** lam_types = (Expr**)malloc(lam_count * sizeof(Expr*));
    
    for (uint32_t i = 0; i < field_count; i++) {
        lam_names[i] = field_names_arr[i];
        lam_types[i] = field_types_arr[i];
    }
    for (uint32_t i = 0; i < ih_count; i++) {
        lam_names[field_count + i] = ih_names[i];
        lam_types[field_count + i] = ih_types[i];
    }
    
    Expr* result = fold_lam_expr(lam_names, lam_types, lam_count, lowered_body);
    
    /* 清理 */
    free(field_actuals);
    free(field_names_arr);
    free(field_types_arr);
    free(param_mapping_keys);
    free(param_mapping_values);
    free(field_mapping_keys);
    free(field_mapping_values);
    free(ih_names);
    free(ih_types);
    free(lam_names);
    free(lam_types);
    elab_local_free(&branch_local);
    
    return result;
}

/* ============================================================
 * elab_lower_match - 完整的 match/case 编译
 * ============================================================ */

static Expr* elab_lower_match(Elaborator* elab, Expr* expr, ElabLocalContext* local, bool is_case) {
    /* 获取家族名 */
    const char* family_name;
    Expr* scrutinee;
    Expr** family_args;
    uint32_t family_arg_count;
    char** bind_names;
    uint32_t bind_name_count;
    Expr* motive_body;
    MatchBranch** branches;
    uint32_t branch_count;
    
    if (is_case) {
        family_name = expr->case_expr.sum_type;
        scrutinee = expr->case_expr.scrutinee;
        family_args = expr->case_expr.type_args;
        family_arg_count = expr->case_expr.type_arg_count;
        bind_names = expr->case_expr.bind_names;
        bind_name_count = expr->case_expr.bind_name_count;
        motive_body = expr->case_expr.motive_body;
        branches = (MatchBranch**)expr->case_expr.branches;
        branch_count = expr->case_expr.branch_count;
    } else {
        family_name = expr->match.inductive;
        scrutinee = expr->match.scrutinee;
        family_args = expr->match.family_args;
        family_arg_count = expr->match.family_arg_count;
        bind_names = expr->match.bind_names;
        bind_name_count = expr->match.bind_name_count;
        motive_body = expr->match.motive_body;
        branches = expr->match.branches;
        branch_count = expr->match.branch_count;
    }
    
    /* 查找归纳类型 */
    InductiveSpec* spec = NULL;
    for (uint32_t i = 0; i < elab->state.inductive_count; i++) {
        if (strcmp(elab->state.inductives[i]->name, family_name) == 0) {
            spec = elab->state.inductives[i];
            break;
        }
    }
    if (spec == NULL) {
        return expr;
    }
    
    /* 精细化 scrutinee 和 family_args */
    Expr* lowered_scrutinee = elab_lower_expr(elab, scrutinee, local);
    Expr** lowered_family_args = (Expr**)malloc(family_arg_count * sizeof(Expr*));
    for (uint32_t i = 0; i < family_arg_count; i++) {
        lowered_family_args[i] = elab_lower_expr(elab, family_args[i], local);
    }
    
    /* 处理 bind_names */
    uint32_t total_bind = spec->param_count + spec->index_count;
    char** bind_actuals = (char**)malloc(total_bind * sizeof(char*));
    
    if (bind_name_count > 0) {
        for (uint32_t i = 0; i < total_bind && i < bind_name_count; i++) {
            if (bind_names[i] != NULL) {
                bind_actuals[i] = strdup(bind_names[i]);
            } else {
                /* 生成默认名 */
                char* default_name = (char*)malloc(16);
                snprintf(default_name, 16, "_b%u", i);
                bind_actuals[i] = default_name;
            }
        }
    } else {
        for (uint32_t i = 0; i < total_bind; i++) {
            char* default_name = (char*)malloc(16);
            snprintf(default_name, 16, "_b%u", i);
            bind_actuals[i] = default_name;
        }
    }
    
    char** param_actuals = bind_actuals;
    char** index_actuals = bind_actuals + spec->param_count;
    
    /* 构建 motive 上下文 */
    ElabLocalContext motive_local;
    elab_local_init(&motive_local);
    for (uint32_t i = 0; i < local->count; i++) {
        elab_local_extend(&motive_local, local->names[i]);
    }
    for (uint32_t i = 0; i < total_bind; i++) {
        elab_local_extend(&motive_local, bind_actuals[i]);
    }
    
    char* alias_name = (is_case ? expr->case_expr.alias : expr->match.alias);
    if (alias_name == NULL) alias_name = strdup("_");
    elab_local_extend(&motive_local, alias_name);
    
    /* 精细化 motive_body */
    Expr* lowered_motive_body = elab_lower_expr(elab, motive_body, &motive_local);
    
    /* 构建 alias_type: Name p1 ... i1 ... */
    /* 在 motive_local 上下文中，bind_actuals 已经被添加为局部变量 */
    Expr** alias_type_args = (Expr**)malloc(total_bind * sizeof(Expr*));
    for (uint32_t i = 0; i < total_bind; i++) {
        alias_type_args[i] = expr_new_atom(strdup(bind_actuals[i]));
    }
    Expr* alias_type = elab_mk_apps(expr_new_atom(strdup(family_name)), alias_type_args, total_bind);
    
    /* 构建 motive Lambda */
    uint32_t motive_param_count = total_bind + 1;
    char** motive_param_names = (char**)malloc(motive_param_count * sizeof(char*));
    Expr** motive_param_types = (Expr**)malloc(motive_param_count * sizeof(Expr*));
    
    /* 构建 param_bindings 和 index_bindings（使用 spec 类型做增量替换） */
    uint32_t tel_cap = spec->param_count + spec->index_count;
    const char** tel_keys = (const char**)malloc(tel_cap * sizeof(const char*));
    Expr** tel_vals = (Expr**)malloc(tel_cap * sizeof(Expr*));
    uint32_t tel_count = 0;
    
    for (uint32_t i = 0; i < spec->param_count; i++) {
        motive_param_names[i] = strdup(param_actuals[i]);
        /* 用之前的参数替换得到类型（与 Python 的 instantiate_telescope 一致） */
        Expr* substituted = substitute_expr(
            spec->param_types[i], tel_keys, tel_vals, tel_count, NULL, NULL);
        /* 确保深拷贝 */
        motive_param_types[i] = (substituted == spec->param_types[i])
            ? expr_deep_copy(spec->param_types[i]) : substituted;
        tel_keys[tel_count] = spec->param_names[i];
        tel_vals[tel_count] = expr_new_atom(strdup(param_actuals[i]));
        tel_count++;
    }
    
    /* 索引类型也需要替换参数名 */
    for (uint32_t i = 0; i < spec->index_count; i++) {
        motive_param_names[spec->param_count + i] = strdup(index_actuals[i]);
        Expr* substituted = substitute_expr(
            spec->index_types[i], tel_keys, tel_vals, tel_count, NULL, NULL);
        motive_param_types[spec->param_count + i] = (substituted == spec->index_types[i])
            ? expr_deep_copy(spec->index_types[i]) : substituted;
        tel_keys[tel_count] = spec->index_names[i];
        tel_vals[tel_count] = expr_new_atom(strdup(index_actuals[i]));
        tel_count++;
    }
    
    free(tel_keys);
    free(tel_vals);
    
    motive_param_names[total_bind] = strdup(alias_name);
    motive_param_types[total_bind] = alias_type;
    
    Expr* motive_expr = fold_lam_expr(motive_param_names, motive_param_types, motive_param_count, lowered_motive_body);
    
    /* 精细化分支 */
    Expr** branch_exprs = (Expr**)malloc(branch_count * sizeof(Expr*));
    for (uint32_t i = 0; i < branch_count; i++) {
        MatchBranch* branch = branches[i];
        
        /* 查找对应的构造器 */
        ElabCtorInfo* ctor = NULL;
        for (uint32_t k = 0; k < spec->ctor_count; k++) {
            if (strcmp(spec->ctor_names[k], branch->ctor) == 0) {
                ctor = &spec->ctors[k];
                break;
            }
        }
        
        if (ctor == NULL) {
            branch_exprs[i] = expr_new_atom(strdup("_"));
            continue;
        }
        
        branch_exprs[i] = build_match_branch(
            elab, spec, ctor, branch, local,
            lowered_family_args, param_actuals, motive_expr, !is_case
        );
    }
    
    /* 构建消去子应用: elim p1 ... motive branch1 ... i1 ... scrutinee */
    Expr** elim_args = (Expr**)malloc((spec->param_count + 1 + branch_count + spec->index_count + 1) * sizeof(Expr*));
    uint32_t arg_idx = 0;
    
    for (uint32_t i = 0; i < spec->param_count; i++) {
        elim_args[arg_idx++] = lowered_family_args[i];
    }
    elim_args[arg_idx++] = motive_expr;
    for (uint32_t i = 0; i < branch_count; i++) {
        elim_args[arg_idx++] = branch_exprs[i];
    }
    for (uint32_t i = 0; i < spec->index_count; i++) {
        elim_args[arg_idx++] = lowered_family_args[spec->param_count + i];
    }
    elim_args[arg_idx++] = lowered_scrutinee;
    
    Expr* elim_expr = expr_new_atom(strdup(spec->eliminator_name));
    Expr* result = elab_mk_apps(elim_expr, elim_args, arg_idx);
    
    /* 清理 */
    free(lowered_family_args);
    free(branch_exprs);
    free(elim_args);
    free(alias_type_args);
    for (uint32_t i = 0; i < total_bind; i++) {
        free(bind_actuals[i]);
    }
    free(bind_actuals);
    for (uint32_t i = 0; i < motive_param_count; i++) {
        free(motive_param_names[i]);
    }
    free(motive_param_names);
    free(motive_param_types);
    elab_local_free(&motive_local);
    
    return result;
}

/* 辅助函数：lower_telescope */
static void lower_telescope(Elaborator* elab, Param** params, uint32_t param_count,
                            ElabLocalContext* local,
                            char*** out_names, Expr*** out_types, 
                            ElabLocalContext* out_local) {
    *out_names = (char**)malloc(param_count * sizeof(char*));
    *out_types = (Expr**)malloc(param_count * sizeof(Expr*));
    
    elab_local_init(out_local);
    for (uint32_t i = 0; i < local->count; i++) {
        elab_local_extend(out_local, local->names[i]);
    }
    
    for (uint32_t i = 0; i < param_count; i++) {
        const char* name = params[i]->name;
        if (name == NULL) {
            name = elab_state_fresh(&elab->state, "p");
        }
        (*out_names)[i] = strdup(name);
        (*out_types)[i] = elab_lower_expr(elab, params[i]->typ, out_local);
        elab_local_extend(out_local, name);
    }
}

/* 精细化表达式 */
Expr* elab_lower_expr(Elaborator* elab, Expr* expr, ElabLocalContext* local) {
    if (expr == NULL) return NULL;
    
    switch (expr->kind) {
        case EXPR_ATOM: {
            if (expr->atom.text == NULL) return mk_atom(elab, "Type");
            if (strcmp(expr->atom.text, "Type") == 0 || strcmp(expr->atom.text, "Prop") == 0) {
                return mk_atom(elab, "Type");
            }
            /* 检查是否是局部变量 */
            for (uint32_t i = 0; i < local->count; i++) {
                if (strcmp(local->names[i], expr->atom.text) == 0) {
                    return expr;  /* 局部变量，直接返回 */
                }
            }
            /* 检查是否是全局名或点号名 */
            if (strchr(expr->atom.text, '.') != NULL) {
                const char* resolved = elab_state_resolve_dot(&elab->state, expr->atom.text);
                if (resolved != NULL) {
                    return mk_atom(elab, resolved);
                }
            }
            if (elab_state_is_defined(&elab->state, expr->atom.text)) {
                return expr;
            }
            /* 未定义的名 */
            return expr;
        }
        
        case EXPR_APP: {
            Expr* func = elab_lower_expr(elab, expr->app.func, local);
            Expr* arg = elab_lower_expr(elab, expr->app.arg, local);
            if (func == expr->app.func && arg == expr->app.arg) return expr;
            return expr_new_app(func, arg);
        }
        
        case EXPR_ARROW: {
            char* name = strdup(elab_state_fresh(&elab->state, "a"));
            Expr* domain = elab_lower_expr(elab, expr->arrow.domain, local);
            ElabLocalContext extended;
            elab_local_init(&extended);
            for (uint32_t i = 0; i < local->count; i++) {
                elab_local_extend(&extended, local->names[i]);
            }
            elab_local_extend(&extended, name);
            Expr* codomain = elab_lower_expr(elab, expr->arrow.codomain, &extended);
            elab_local_free(&extended);
            return expr_new_pi(name, domain, codomain);
        }
        
        case EXPR_PI: {
            char* name = strdup(expr->pi.name ? expr->pi.name : "_");
            Expr* domain = elab_lower_expr(elab, expr->pi.domain, local);
            ElabLocalContext extended;
            elab_local_init(&extended);
            for (uint32_t i = 0; i < local->count; i++) {
                elab_local_extend(&extended, local->names[i]);
            }
            elab_local_extend(&extended, name);
            Expr* codomain = elab_lower_expr(elab, expr->pi.codomain, &extended);
            elab_local_free(&extended);
            return expr_new_pi(name, domain, codomain);
        }
        
        case EXPR_LAMBDA: {
            char* name = strdup(expr->lambda.param->name ? expr->lambda.param->name : "_");
            Expr* param_type = elab_lower_expr(elab, expr->lambda.param->typ, local);
            ElabLocalContext extended;
            elab_local_init(&extended);
            for (uint32_t i = 0; i < local->count; i++) {
                elab_local_extend(&extended, local->names[i]);
            }
            elab_local_extend(&extended, name);
            Expr* body = elab_lower_expr(elab, expr->lambda.body, &extended);
            elab_local_free(&extended);
            Param* p = param_new(name, param_type);
            return expr_new_lambda(p, body);
        }
        
        case EXPR_LET: {
            char* name = strdup(expr->let.name ? expr->let.name : "_");
            Expr* typ = elab_lower_expr(elab, expr->let.typ, local);
            Expr* value = elab_lower_expr(elab, expr->let.value, local);
            ElabLocalContext extended;
            elab_local_init(&extended);
            for (uint32_t i = 0; i < local->count; i++) {
                elab_local_extend(&extended, local->names[i]);
            }
            elab_local_extend(&extended, name);
            Expr* body = elab_lower_expr(elab, expr->let.body, &extended);
            elab_local_free(&extended);
            return expr_new_let(name, typ, value, body);
        }
        
        case EXPR_EQ: {
            Expr* typ = elab_lower_expr(elab, expr->eq.typ, local);
            Expr* lhs = elab_lower_expr(elab, expr->eq.lhs, local);
            Expr* rhs = elab_lower_expr(elab, expr->eq.rhs, local);
            return expr_new_eq(typ, lhs, rhs);
        }
        
        case EXPR_MATCH: {
            /* Match 表达式精细化：编译为消去子应用 */
            /* 简化版：编译为 eliminator 应用 */
            return elab_lower_match(elab, expr, local, false);
        }
        
        case EXPR_CASE: {
            /* Case 表达式精细化：编译为消去子应用 */
            /* 简化版：编译为 eliminator 应用 */
            return elab_lower_match(elab, expr, local, true);
        }
        
        case EXPR_PRODUCT: {
            /* Product 表达式精细化：转换为构造器应用 */
            InductiveSpec* spec = NULL;
            for (uint32_t i = 0; i < elab->state.inductive_count; i++) {
                if (strcmp(elab->state.inductives[i]->name, expr->product.type_name) == 0) {
                    spec = elab->state.inductives[i];
                    break;
                }
            }
            if (spec == NULL || strcmp(spec->kind, "product") != 0) {
                return expr;
            }
            /* 构造器应用: spec.constructor_names[0] arg1 arg2 ... */
            Expr* result = expr_new_atom(strdup(spec->ctor_names[0]));
            for (uint32_t i = 0; i < expr->product.arg_count; i++) {
                Expr* arg = elab_lower_expr(elab, expr->product.args[i], local);
                result = expr_new_app(result, arg);
            }
            return result;
        }
        
        default:
            return expr;
    }
}

/* 精细化 var 声明 */
static RDecl* elab_var_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    ElabLocalContext local;
    elab_local_init(&local);
    
    RDecl* rdecl = (RDecl*)calloc(1, sizeof(RDecl));
    rdecl->kind = RDECL_DEFINITION;
    rdecl->def.name = strdup(decl->var_decl.name);
    rdecl->def.typ = elab_lower_expr(elab, decl->var_decl.typ, &local);
    rdecl->def.value = elab_lower_expr(elab, decl->var_decl.value, &local);
    rdecl->def.kind = strdup(decl->var_decl.kind_str);
    
    elab_local_free(&local);
    *out_count = 1;
    return rdecl;
}

/* 精细化 fun 声明 */
static RDecl* elab_fun_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    ElabLocalContext local;
    elab_local_init(&local);
    
    char** param_names;
    Expr** param_types;
    ElabLocalContext extended;
    lower_telescope(elab, decl->fun_decl.params, decl->fun_decl.param_count, 
                    &local, &param_names, &param_types, &extended);
    
    Expr* ret_type = elab_lower_expr(elab, decl->fun_decl.ret_type, &extended);
    Expr* body = elab_lower_expr(elab, decl->fun_decl.body, &extended);
    
    /* 构建 Pi 类型 */
    Expr* typ = fold_pi_expr(param_names, param_types, decl->fun_decl.param_count, ret_type);
    
    /* 构建 Lambda 体 */
    Expr* value = fold_lam_expr(param_names, param_types, decl->fun_decl.param_count, body);
    
    RDecl* rdecl = (RDecl*)calloc(1, sizeof(RDecl));
    rdecl->kind = RDECL_DEFINITION;
    rdecl->def.name = strdup(decl->fun_decl.name);
    rdecl->def.typ = typ;
    rdecl->def.value = value;
    rdecl->def.kind = strdup(decl->fun_decl.kind_str);
    
    elab_local_free(&extended);
    elab_local_free(&local);
    *out_count = 1;
    return rdecl;
}

/* ============================================================
 * materialize_family - 归纳类型完整展开
 * ============================================================ */

/* 构建消去子的 motive 类型 */
static Expr* build_motive_type(Elaborator* elab, InductiveSpec* spec) {
    /* motive: (p1: P1) -> ... -> (i1: I1) -> ... -> (q: Name p1 ... i1 ...) -> Type */
    const char* q_name = elab_state_fresh(&elab->state, "q");
    
    /* 构建 q 的类型: Name p1 ... i1 ... */
    Expr** type_args = (Expr**)malloc((spec->param_count + spec->index_count) * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->param_count; i++) {
        type_args[i] = expr_new_atom(strdup(spec->param_names[i]));
    }
    for (uint32_t i = 0; i < spec->index_count; i++) {
        type_args[spec->param_count + i] = expr_new_atom(strdup(spec->index_names[i]));
    }
    Expr* q_type = mk_apps(expr_new_atom(strdup(spec->name)), type_args, spec->param_count + spec->index_count);
    free(type_args);
    
    /* 构建 telescope + (q: q_type) */
    uint32_t total_count = spec->param_count + spec->index_count + 1;
    char** names = (char**)malloc(total_count * sizeof(char*));
    Expr** types = (Expr**)malloc(total_count * sizeof(Expr*));
    
    for (uint32_t i = 0; i < spec->param_count; i++) {
        names[i] = strdup(spec->param_names[i]);
        types[i] = spec->param_types[i];
    }
    for (uint32_t i = 0; i < spec->index_count; i++) {
        names[spec->param_count + i] = strdup(spec->index_names[i]);
        types[spec->param_count + i] = spec->index_types[i];
    }
    names[total_count - 1] = strdup(q_name);
    types[total_count - 1] = q_type;
    
    Expr* result = fold_pi_expr(names, types, total_count, expr_new_atom(strdup("Type")));
    
    free(names);
    free(types);
    
    return result;
}

/* 构建消去子分支类型 */
static Expr* build_eliminator_branch_type(Elaborator* elab, InductiveSpec* spec, 
                                           ElabCtorInfo* ctor, const char* motive_name) {
    /* 构造器表达式: Ctor p1 ... field1 field2 ... */
    Expr** ctor_args = (Expr**)malloc((spec->param_count + ctor->field_count) * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->param_count; i++) {
        ctor_args[i] = expr_new_atom(strdup(spec->param_names[i]));
    }
    for (uint32_t i = 0; i < ctor->field_count; i++) {
        ctor_args[spec->param_count + i] = expr_new_atom(strdup(ctor->field_names[i]));
    }
    Expr* ctor_expr = mk_apps(expr_new_atom(strdup(ctor->name)), ctor_args, spec->param_count + ctor->field_count);
    free(ctor_args);
    
    /* 结果类型: motive p1 ... target_indices ... (Ctor p1 ... field1 field2 ...) */
    /* target_indices 来自构造器的 target_index_exprs，需要用字段名替换参数名 */
    uint32_t result_total = spec->param_count + spec->index_count + 1;
    Expr** result_args = (Expr**)malloc(result_total * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->param_count; i++) {
        result_args[i] = expr_new_atom(strdup(spec->param_names[i]));
    }
    /* 使用构造器的 target_index_exprs，通过替换参数名为参数变量来实例化 */
    if (ctor->target_index_count == spec->index_count && ctor->target_index_exprs != NULL) {
        /* 构建替换映射：param_name -> param_atom */
        const char** mapping_keys = (const char**)malloc(spec->param_count * sizeof(const char*));
        Expr** mapping_values = (Expr**)malloc(spec->param_count * sizeof(Expr*));
        for (uint32_t i = 0; i < spec->param_count; i++) {
            mapping_keys[i] = spec->param_names[i];
            mapping_values[i] = expr_new_atom(strdup(spec->param_names[i]));
        }
        for (uint32_t i = 0; i < spec->index_count; i++) {
            result_args[spec->param_count + i] = substitute_expr(
                ctor->target_index_exprs[i], mapping_keys, mapping_values, spec->param_count,
                NULL, NULL);
        }
        free(mapping_keys);
        free(mapping_values);
    } else {
        for (uint32_t i = 0; i < spec->index_count; i++) {
            result_args[spec->param_count + i] = expr_new_atom(strdup(spec->index_names[i]));
        }
    }
    result_args[spec->param_count + spec->index_count] = ctor_expr;
    Expr* result = mk_apps(expr_new_atom(strdup(motive_name)), result_args, result_total);
    free(result_args);
    
    /* 如果是归纳类型，添加归纳假设 */
    if (strcmp(spec->kind, "inductive") == 0 && ctor->recursive_field_count > 0) {
        /* 构建归纳假设绑定器 */
        char** ih_names = (char**)malloc(ctor->recursive_field_count * sizeof(char*));
        Expr** ih_types = (Expr**)malloc(ctor->recursive_field_count * sizeof(Expr*));
        
        for (uint32_t i = 0; i < ctor->recursive_field_count; i++) {
            ElabRecFieldInfo* rec = &ctor->recursive_fields[i];
            char* field_name = ctor->field_names[rec->field_position];
            
            /* IH 名称：ih_field_name */
            char ih_name[256];
            snprintf(ih_name, sizeof(ih_name), "ih_%s", field_name);
            ih_names[i] = strdup(ih_name);
            
            /* IH 类型：motive p1 ... i1 ... field_name */
            Expr** ih_args = (Expr**)malloc((spec->param_count + spec->index_count + 1) * sizeof(Expr*));
            for (uint32_t j = 0; j < spec->param_count; j++) {
                ih_args[j] = expr_new_atom(strdup(spec->param_names[j]));
            }
            for (uint32_t j = 0; j < spec->index_count; j++) {
                ih_args[spec->param_count + j] = expr_new_atom(strdup(spec->index_names[j]));
            }
            ih_args[spec->param_count + spec->index_count] = expr_new_atom(strdup(field_name));
            ih_types[i] = mk_apps(expr_new_atom(strdup(motive_name)), ih_args, spec->param_count + spec->index_count + 1);
            free(ih_args);
        }
        
        /* 封装归纳假设: ih1: IH1 -> ih2: IH2 -> ... -> result */
        result = fold_pi_expr(ih_names, ih_types, ctor->recursive_field_count, result);
        
        for (uint32_t i = 0; i < ctor->recursive_field_count; i++) {
            free(ih_names[i]);
        }
        free(ih_names);
        free(ih_types);
    }
    
    /* 封装字段参数: (field1: T1) -> (field2: T2) -> ... -> result */
    Expr* branch_type = fold_pi_expr(ctor->field_names, ctor->field_types, ctor->field_count, result);
    
    return branch_type;
}

/* 构建消去子完整类型 */
static Expr* build_eliminator_type(Elaborator* elab, InductiveSpec* spec, Expr* motive_type) {
    /* elim: (p1: P1) -> ... -> (motive: motive_type) -> (branch_1: ...) -> ... -> (i1: I1) -> ... -> (q: Name p1 ... i1 ...) -> motive p1 ... i1 ... q */
    const char* motive_name = elab_state_fresh(&elab->state, "motive");
    const char* q_name = elab_state_fresh(&elab->state, "q");
    
    /* 构建分支绑定器 */
    uint32_t branch_count = spec->ctor_count;
    char** branch_names = (char**)malloc(branch_count * sizeof(char*));
    Expr** branch_types = (Expr**)malloc(branch_count * sizeof(Expr*));
    
    for (uint32_t i = 0; i < branch_count; i++) {
        char name[256];
        snprintf(name, sizeof(name), "branch_%s", spec->ctor_names[i]);
        branch_names[i] = strdup(name);
        branch_types[i] = build_eliminator_branch_type(elab, spec, &spec->ctors[i], motive_name);
    }
    
    /* 构建结果类型: motive p1 ... i1 ... q */
    Expr** result_args = (Expr**)malloc((spec->param_count + spec->index_count + 1) * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->param_count; i++) {
        result_args[i] = expr_new_atom(strdup(spec->param_names[i]));
    }
    for (uint32_t i = 0; i < spec->index_count; i++) {
        result_args[spec->param_count + i] = expr_new_atom(strdup(spec->index_names[i]));
    }
    result_args[spec->param_count + spec->index_count] = expr_new_atom(strdup(q_name));
    Expr* result = mk_apps(expr_new_atom(strdup(motive_name)), result_args, spec->param_count + spec->index_count + 1);
    free(result_args);
    
    /* 构建 q 的类型 */
    Expr** q_type_args = (Expr**)malloc((spec->param_count + spec->index_count) * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->param_count; i++) {
        q_type_args[i] = expr_new_atom(strdup(spec->param_names[i]));
    }
    for (uint32_t i = 0; i < spec->index_count; i++) {
        q_type_args[spec->param_count + i] = expr_new_atom(strdup(spec->index_names[i]));
    }
    Expr* q_type = mk_apps(expr_new_atom(strdup(spec->name)), q_type_args, spec->param_count + spec->index_count);
    free(q_type_args);
    
    /* 构建 body: (i1: I1) -> ... -> (q: q_type) -> result */
    uint32_t body_count = spec->index_count + 1;
    char** body_names = (char**)malloc(body_count * sizeof(char*));
    Expr** body_types = (Expr**)malloc(body_count * sizeof(Expr*));
    for (uint32_t i = 0; i < spec->index_count; i++) {
        body_names[i] = strdup(spec->index_names[i]);
        body_types[i] = spec->index_types[i];
    }
    body_names[body_count - 1] = strdup(q_name);
    body_types[body_count - 1] = q_type;
    Expr* body = fold_pi_expr(body_names, body_types, body_count, result);
    free(body_names);
    free(body_types);
    
    /* 构建完整类型: (p1: P1) -> ... -> (motive: motive_type) -> (branch_1: ...) -> ... -> body */
    uint32_t total_count = spec->param_count + 1 + branch_count;
    char** all_names = (char**)malloc(total_count * sizeof(char*));
    Expr** all_types = (Expr**)malloc(total_count * sizeof(Expr*));
    
    for (uint32_t i = 0; i < spec->param_count; i++) {
        all_names[i] = strdup(spec->param_names[i]);
        all_types[i] = spec->param_types[i];
    }
    all_names[spec->param_count] = strdup(motive_name);
    all_types[spec->param_count] = motive_type;
    for (uint32_t i = 0; i < branch_count; i++) {
        all_names[spec->param_count + 1 + i] = branch_names[i];
        all_types[spec->param_count + 1 + i] = branch_types[i];
    }
    
    Expr* elim_type = fold_pi_expr(all_names, all_types, total_count, body);
    
    free(all_names);
    free(all_types);
    free(branch_names);
    free(branch_types);
    
    return elim_type;
}

/* materialize_family - 归纳类型完整展开 */
static RDecl* materialize_family(Elaborator* elab, InductiveSpec* spec, uint32_t* out_count) {
    /* 注册全局名 */
    elab_state_register_global(&elab->state, spec->name, "type_ctor");
    for (uint32_t i = 0; i < spec->ctor_count; i++) {
        elab_state_register_global(&elab->state, spec->ctor_names[i], "data_ctor");
    }
    elab_state_register_global(&elab->state, spec->eliminator_name, "eliminator");
    
    /* 保存 spec */
    if (elab->state.inductive_count < MAX_INDUCTIVES) {
        elab->state.inductives[elab->state.inductive_count++] = spec;
    }
    
    /* 计算声明数量: type_ctor + data_ctors + eliminator */
    uint32_t count = 1 + spec->ctor_count + 1;
    RDecl* rdecls = (RDecl*)calloc(count, sizeof(RDecl));
    
    /* 类型构造器 */
    rdecls[0].kind = RDECL_TYPE_CTOR;
    rdecls[0].type_ctor.name = strdup(spec->name);
    rdecls[0].type_ctor.kind = strdup("type_ctor");
    rdecls[0].type_ctor.family_kind = strdup(spec->kind);
    rdecls[0].type_ctor.param_names = spec->param_names;
    rdecls[0].type_ctor.param_types = spec->param_types;
    rdecls[0].type_ctor.param_count = spec->param_count;
    rdecls[0].type_ctor.index_names = spec->index_names;
    rdecls[0].type_ctor.index_types = spec->index_types;
    rdecls[0].type_ctor.index_count = spec->index_count;
    rdecls[0].type_ctor.constructor_names = spec->ctor_names;
    rdecls[0].type_ctor.constructor_count = spec->ctor_count;
    
    /* 构建 type_ctor 的类型: (p1: P1) -> ... -> (i1: I1) -> ... -> Type */
    {
        uint32_t total = spec->param_count + spec->index_count;
        char** typ_names = (char**)malloc(total * sizeof(char*));
        Expr** typ_types = (Expr**)malloc(total * sizeof(Expr*));
        for (uint32_t i = 0; i < spec->param_count; i++) {
            typ_names[i] = spec->param_names[i];
            typ_types[i] = spec->param_types[i];
        }
        for (uint32_t i = 0; i < spec->index_count; i++) {
            typ_names[spec->param_count + i] = spec->index_names[i];
            typ_types[spec->param_count + i] = spec->index_types[i];
        }
        rdecls[0].type_ctor.typ = fold_pi_expr(typ_names, typ_types, total, expr_new_atom(strdup("Type")));
        free(typ_names);
        free(typ_types);
    }
    
    /* 构造器类型: (p1: P1) -> ... -> (field1: T1) -> ... -> Name p1 ... i1 ... */
    for (uint32_t i = 0; i < spec->ctor_count; i++) {
        ElabCtorInfo* ctor = &spec->ctors[i];
        
        /* 构建构造器参数列表 */
        uint32_t param_count = spec->param_count + ctor->field_count;
        char** param_names = (char**)malloc(param_count * sizeof(char*));
        Expr** param_types_arr = (Expr**)malloc(param_count * sizeof(Expr*));
        
        for (uint32_t j = 0; j < spec->param_count; j++) {
            param_names[j] = strdup(spec->param_names[j]);
            param_types_arr[j] = spec->param_types[j];
        }
        for (uint32_t j = 0; j < ctor->field_count; j++) {
            param_names[spec->param_count + j] = strdup(ctor->field_names[j]);
            param_types_arr[spec->param_count + j] = ctor->field_types[j];
        }
        
        /* 构建结果类型: Name p1 ... i1 ... */
        Expr** result_args = (Expr**)malloc((spec->param_count + spec->index_count) * sizeof(Expr*));
        for (uint32_t j = 0; j < spec->param_count; j++) {
            result_args[j] = expr_new_atom(strdup(spec->param_names[j]));
        }
        for (uint32_t j = 0; j < spec->index_count; j++) {
            if (ctor->target_index_exprs != NULL && j < ctor->target_index_count) {
                result_args[spec->param_count + j] = ctor->target_index_exprs[j];
            } else {
                result_args[spec->param_count + j] = expr_new_atom(strdup("_"));
            }
        }
        Expr* result = mk_apps(expr_new_atom(strdup(spec->name)), result_args, spec->param_count + spec->index_count);
        free(result_args);
        
        Expr* ctor_type = fold_pi_expr(param_names, param_types_arr, param_count, result);
        
        free(param_names);
        free(param_types_arr);
        
        rdecls[1 + i].kind = RDECL_DATA_CTOR;
        rdecls[1 + i].data_ctor.name = strdup(ctor->name);
        rdecls[1 + i].data_ctor.owner = strdup(spec->name);
        rdecls[1 + i].data_ctor.typ = ctor_type;
        rdecls[1 + i].data_ctor.kind = strdup("data_ctor");
        /* param_names 包含类型参数 + 字段（与 Python 的 constructor_parameter_list 一致） */
        rdecls[1 + i].data_ctor.param_count = param_count;  /* spec->param_count + ctor->field_count */
        rdecls[1 + i].data_ctor.param_names = (char**)malloc(param_count * sizeof(char*));
        rdecls[1 + i].data_ctor.param_types = (Expr**)malloc(param_count * sizeof(Expr*));
        for (uint32_t j = 0; j < spec->param_count; j++) {
            rdecls[1 + i].data_ctor.param_names[j] = strdup(spec->param_names[j]);
            rdecls[1 + i].data_ctor.param_types[j] = spec->param_types[j];
        }
        for (uint32_t j = 0; j < ctor->field_count; j++) {
            rdecls[1 + i].data_ctor.param_names[spec->param_count + j] = strdup(ctor->field_names[j]);
            rdecls[1 + i].data_ctor.param_types[spec->param_count + j] = ctor->field_types[j];
        }
        rdecls[1 + i].data_ctor.target_index_exprs = ctor->target_index_exprs;
        rdecls[1 + i].data_ctor.target_index_count = ctor->target_index_count;
        rdecls[1 + i].data_ctor.spec_param_count = spec->param_count;
        rdecls[1 + i].data_ctor.recursive_field_count = ctor->recursive_field_count;
        if (ctor->recursive_field_count > 0) {
            rdecls[1 + i].data_ctor.recursive_fields = (RRecFieldInfo*)calloc(ctor->recursive_field_count, sizeof(RRecFieldInfo));
            for (uint32_t j = 0; j < ctor->recursive_field_count; j++) {
                rdecls[1 + i].data_ctor.recursive_fields[j].field_position = ctor->recursive_fields[j].field_position;
                rdecls[1 + i].data_ctor.recursive_fields[j].recursive_kind = strdup(ctor->recursive_fields[j].recursive_kind);
                rdecls[1 + i].data_ctor.recursive_fields[j].ho_names = ctor->recursive_fields[j].ho_names;
                rdecls[1 + i].data_ctor.recursive_fields[j].ho_types = ctor->recursive_fields[j].ho_types;
                rdecls[1 + i].data_ctor.recursive_fields[j].ho_count = ctor->recursive_fields[j].ho_count;
                rdecls[1 + i].data_ctor.recursive_fields[j].target_index_exprs = ctor->recursive_fields[j].target_index_exprs;
                rdecls[1 + i].data_ctor.recursive_fields[j].target_index_count = ctor->recursive_fields[j].target_index_count;
            }
        } else {
            rdecls[1 + i].data_ctor.recursive_fields = NULL;
        }
    }
    
    /* 消去子 */
    Expr* motive_type = build_motive_type(elab, spec);
    Expr* elim_type = build_eliminator_type(elab, spec, motive_type);
    
    rdecls[count - 1].kind = RDECL_ELIMINATOR;
    rdecls[count - 1].eliminator.name = strdup(spec->eliminator_name);
    rdecls[count - 1].eliminator.owner = strdup(spec->name);
    rdecls[count - 1].eliminator.motive_type = motive_type;
    rdecls[count - 1].eliminator.typ = elim_type;
    rdecls[count - 1].eliminator.kind = strdup(strcmp(spec->kind, "inductive") == 0 ? "rec" : "elim");
    rdecls[count - 1].eliminator.branch_count = spec->ctor_count;
    rdecls[count - 1].eliminator.branch_order = (char**)malloc(spec->ctor_count * sizeof(char*));
    for (uint32_t i = 0; i < spec->ctor_count; i++) {
        rdecls[count - 1].eliminator.branch_order[i] = strdup(spec->ctor_names[i]);
    }
    
    *out_count = count;
    return rdecls;
}

/* 精细化 axiom 声明 */
static RDecl* elab_axiom_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    ElabLocalContext local;
    elab_local_init(&local);
    
    RDecl* rdecl = (RDecl*)calloc(1, sizeof(RDecl));
    rdecl->kind = RDECL_DEFINITION;
    rdecl->def.name = strdup(decl->axiom_decl.name);
    rdecl->def.typ = elab_lower_expr(elab, decl->axiom_decl.typ, &local);
    rdecl->def.value = NULL;
    rdecl->def.kind = strdup("axiom");
    
    elab_local_free(&local);
    *out_count = 1;
    return rdecl;
}

/* 精细化 inductive 声明 */
static RDecl* elab_inductive_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    /* 构建 InductiveSpec */
    InductiveSpec* spec = (InductiveSpec*)calloc(1, sizeof(InductiveSpec));
    spec->name = strdup(decl->inductive_decl.name);
    spec->kind = strdup(decl->inductive_decl.kind_str);
    
    /* 参数 telescope */
    spec->param_count = decl->inductive_decl.param_count;
    if (spec->param_count > 0) {
        spec->param_names = (char**)malloc(spec->param_count * sizeof(char*));
        spec->param_types = (Expr**)malloc(spec->param_count * sizeof(Expr*));
        for (uint32_t i = 0; i < spec->param_count; i++) {
            spec->param_names[i] = strdup(decl->inductive_decl.params[i]->name);
            spec->param_types[i] = elab_lower_expr(elab, decl->inductive_decl.params[i]->typ, &(ElabLocalContext){0});
        }
    }
    
    /* 解析 arity 获取 index_telescope */
    Expr* arity = NULL;
    if (decl->inductive_decl.arity != NULL) {
        arity = elab_lower_expr(elab, decl->inductive_decl.arity, &(ElabLocalContext){0});
    } else {
        arity = expr_new_atom(strdup("Type"));
    }
    
    /* 拆解 Pi 类型获取 index_telescope */
    PiTelescope tel = split_pi_expr(arity);
    spec->index_count = tel.count;
    if (tel.count > 0) {
        spec->index_names = tel.names;
        spec->index_types = tel.types;
    } else {
        spec->index_names = NULL;
        spec->index_types = NULL;
    }
    
    /* 消去子名称 */
    char elim_name[256];
    if (strcmp(decl->inductive_decl.kind_str, "inductive") == 0) {
        snprintf(elim_name, sizeof(elim_name), "%s.rec", decl->inductive_decl.name);
    } else {
        snprintf(elim_name, sizeof(elim_name), "%s.elim", decl->inductive_decl.name);
    }
    spec->eliminator_name = strdup(elim_name);
    
    /* 构造器 */
    spec->ctor_count = decl->inductive_decl.ctor_count;
    spec->ctor_names = (char**)malloc(spec->ctor_count * sizeof(char*));
    spec->ctors = (ElabCtorInfo*)calloc(spec->ctor_count, sizeof(ElabCtorInfo));
    
    for (uint32_t i = 0; i < spec->ctor_count; i++) {
        CtorDecl* ctor_decl = decl->inductive_decl.ctors[i];
        spec->ctor_names[i] = strdup(ctor_decl->name);
        
        ElabCtorInfo* ctor = &spec->ctors[i];
        ctor->name = strdup(ctor_decl->name);
        
        /* 解析构造器类型为字段列表 */
        Expr* ctor_type = elab_lower_expr(elab, ctor_decl->typ, &(ElabLocalContext){0});
        PiTelescope ctor_tel = split_pi_expr(ctor_type);
        
        /* 检查构造器返回类型是否是正确的归纳类型 */
        /* 简化版：暂时不验证 */
        
        ctor->field_count = ctor_tel.count;
        if (ctor_tel.count > 0) {
            ctor->field_names = ctor_tel.names;
            ctor->field_types = ctor_tel.types;
        } else {
            ctor->field_names = NULL;
            ctor->field_types = NULL;
        }
        
        /* 分析递归字段 */
        ctor->recursive_fields = compile_recursive_fields(
            decl->inductive_decl.kind_str,
            decl->inductive_decl.name,
            (const char**)spec->param_names,
            spec->param_count,
            spec->index_count,
            ctor->field_types,
            ctor->field_count,
            &ctor->recursive_field_count
        );
        
        /* 提取 target_index_exprs：构造器返回类型中参数之后的索引参数 */
        if (spec->index_count > 0) {
            Expr* ctor_type_expr = ctor_tel.body;  /* 构造器的返回类型 */
            if (ctor_type_expr != NULL && ctor_type_expr->kind == EXPR_APP) {
                /* 展开应用链: Name p1 ... i1 ... */
                Expr* head = ctor_type_expr;
                Expr* result_args[64];
                uint32_t result_arg_count = 0;
                while (head->kind == EXPR_APP && result_arg_count < 64) {
                    result_args[result_arg_count++] = head->app.arg;
                    head = head->app.func;
                }
                if (head->kind == EXPR_ATOM && strcmp(head->atom.text, decl->inductive_decl.name) == 0) {
                    /* result_args 是反向的，需要反转 */
                    for (uint32_t j = 0; j < result_arg_count / 2; j++) {
                        Expr* tmp = result_args[j];
                        result_args[j] = result_args[result_arg_count - 1 - j];
                        result_args[result_arg_count - 1 - j] = tmp;
                    }
                    /* 跳过参数，提取索引 */
                    if (result_arg_count >= spec->param_count + spec->index_count) {
                        ctor->target_index_count = spec->index_count;
                        ctor->target_index_exprs = (Expr**)malloc(spec->index_count * sizeof(Expr*));
                        for (uint32_t j = 0; j < spec->index_count; j++) {
                            ctor->target_index_exprs[j] = result_args[spec->param_count + j];
                        }
                    }
                }
            }
        }
        
        if (ctor->target_index_count == 0 && spec->index_count > 0) {
            /* 如果无法提取，用默认值 */
            ctor->target_index_count = spec->index_count;
            ctor->target_index_exprs = (Expr**)malloc(spec->index_count * sizeof(Expr*));
            for (uint32_t j = 0; j < spec->index_count; j++) {
                ctor->target_index_exprs[j] = expr_new_atom(strdup("_"));
            }
        }
    }
    
    /* 使用 materialize_family 展开 */
    RDecl* rdecls = materialize_family(elab, spec, out_count);
    
    return rdecls;
}

/* 精细化 product 声明 */
static RDecl* elab_product_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    /* Product 类型编译为归纳类型家族 */
    /* product Name { field1: T1, field2: T2 } 编译为：
     * inductive Name {
     *   | Name.mk: (field1: T1) -> (field2: T2) -> Name
     * };
     * Name.elim: (motive: Name -> Type) -> ((field1: T1) -> (field2: T2) -> motive (Name.mk field1 field2)) -> (n: Name) -> motive n
     * Name.field1: Name -> T1
     * Name.field2: Name -> T2
     */
    
    const char* name = decl->product_decl.name;
    uint32_t field_count = decl->product_decl.field_count;
    
    /* 注册全局名 */
    elab_state_register_global(&elab->state, name, "type_ctor");
    
    char mk_name[256];
    snprintf(mk_name, sizeof(mk_name), "%s.mk", name);
    elab_state_register_global(&elab->state, mk_name, "data_ctor");
    
    char elim_name[256];
    snprintf(elim_name, sizeof(elim_name), "%s.elim", name);
    elab_state_register_global(&elab->state, elim_name, "eliminator");
    
    /* 计算声明数量：type_ctor + data_ctor + eliminator + projections */
    uint32_t count = 3 + field_count;
    RDecl* rdecls = (RDecl*)calloc(count, sizeof(RDecl));
    
    /* 类型构造器 */
    rdecls[0].kind = RDECL_TYPE_CTOR;
    rdecls[0].type_ctor.name = strdup(name);
    rdecls[0].type_ctor.kind = strdup("type_ctor");
    rdecls[0].type_ctor.family_kind = strdup("product");
    rdecls[0].type_ctor.constructor_names = (char**)malloc(sizeof(char*));
    rdecls[0].type_ctor.constructor_names[0] = strdup(mk_name);
    rdecls[0].type_ctor.constructor_count = 1;
    
    /* 数据构造器：Name.mk: (field1: T1) -> (field2: T2) -> Name */
    ElabLocalContext local;
    elab_local_init(&local);
    
    char** param_names = (char**)malloc(field_count * sizeof(char*));
    Expr** param_types = (Expr**)malloc(field_count * sizeof(Expr*));
    
    for (uint32_t i = 0; i < field_count; i++) {
        param_names[i] = strdup(decl->product_decl.fields[i]->name);
        param_types[i] = elab_lower_expr(elab, decl->product_decl.fields[i]->typ, &local);
        elab_local_extend(&local, param_names[i]);
    }
    
    /* 构造器类型：(field1: T1) -> (field2: T2) -> Name */
    Expr* ctor_result = mk_atom(elab, name);
    Expr* ctor_type = fold_pi_expr(param_names, param_types, field_count, ctor_result);
    
    rdecls[1].kind = RDECL_DATA_CTOR;
    rdecls[1].data_ctor.name = strdup(mk_name);
    rdecls[1].data_ctor.owner = strdup(name);
    rdecls[1].data_ctor.typ = ctor_type;
    rdecls[1].data_ctor.kind = strdup("data_ctor");
    rdecls[1].data_ctor.param_names = param_names;
    rdecls[1].data_ctor.param_types = param_types;
    rdecls[1].data_ctor.param_count = field_count;
    
    /* 消去子 */
    rdecls[2].kind = RDECL_ELIMINATOR;
    rdecls[2].eliminator.name = strdup(elim_name);
    rdecls[2].eliminator.owner = strdup(name);
    rdecls[2].eliminator.kind = strdup("elim");
    rdecls[2].eliminator.branch_count = 1;
    rdecls[2].eliminator.branch_order = (char**)malloc(sizeof(char*));
    rdecls[2].eliminator.branch_order[0] = strdup(mk_name);
    
    /* 投影函数 */
    for (uint32_t i = 0; i < field_count; i++) {
        char proj_name[256];
        snprintf(proj_name, sizeof(proj_name), "%s.%s", name, decl->product_decl.fields[i]->name);
        elab_state_register_global(&elab->state, proj_name, "projection");
        
        /* 投影类型：Name -> Ti */
        Expr* proj_type = expr_new_pi(strdup("_"), mk_atom(elab, name), param_types[i]);
        
        rdecls[3 + i].kind = RDECL_DEFINITION;
        rdecls[3 + i].def.name = strdup(proj_name);
        rdecls[3 + i].def.typ = proj_type;
        rdecls[3 + i].def.value = NULL;  /* 投影函数的值由消去子定义 */
        rdecls[3 + i].def.kind = strdup("projection");
    }
    
    elab_local_free(&local);
    
    *out_count = count;
    return rdecls;
}

/* 精细化 example 声明 */
static RDecl* elab_example_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    ElabLocalContext local;
    elab_local_init(&local);
    
    /* 生成唯一的 example 名称 */
    const char* name = elab_state_fresh_example_name(&elab->state);
    
    RDecl* rdecl = (RDecl*)calloc(1, sizeof(RDecl));
    rdecl->kind = RDECL_DEFINITION;
    rdecl->def.name = strdup(name);
    rdecl->def.typ = elab_lower_expr(elab, decl->example_decl.typ, &local);
    rdecl->def.value = elab_lower_expr(elab, decl->example_decl.value, &local);
    rdecl->def.kind = strdup("var");
    
    elab_local_free(&local);
    *out_count = 1;
    return rdecl;
}

/* 精细化 equation 声明 */
static RDecl* elab_equation_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    ElabLocalContext local;
    elab_local_init(&local);
    
    /* 获取名称和类型 */
    const char* name = decl->var_decl.name;
    Expr* typ = elab_lower_expr(elab, decl->var_decl.typ, &local);
    
    /* 拆解 Pi 类型获取 telescope */
    PiTelescope tel = split_pi_expr(typ);
    
    /* 构建 Lambda 封装 */
    char** param_names = (char**)malloc(tel.count * sizeof(char*));
    Expr** param_types = (Expr**)malloc(tel.count * sizeof(Expr*));
    ElabLocalContext extended;
    elab_local_init(&extended);
    
    for (uint32_t i = 0; i < tel.count; i++) {
        param_names[i] = tel.names[i] ? strdup(tel.names[i]) : strdup("_");
        param_types[i] = tel.types[i];
        elab_local_extend(&extended, param_names[i]);
    }
    
    Expr* body = elab_lower_expr(elab, decl->var_decl.value, &extended);
    Expr* value = fold_lam_expr(param_names, param_types, tel.count, body);
    
    RDecl* rdecl = (RDecl*)calloc(1, sizeof(RDecl));
    rdecl->kind = RDECL_DEFINITION;
    rdecl->def.name = strdup(name);
    rdecl->def.typ = typ;
    rdecl->def.value = value;
    rdecl->def.kind = strdup("var");
    
    /* 清理 */
    for (uint32_t i = 0; i < tel.count; i++) {
        free(param_names[i]);
    }
    free(param_names);
    free(param_types);
    pi_telescope_free(&tel);
    elab_local_free(&extended);
    elab_local_free(&local);
    
    *out_count = 1;
    return rdecl;
}

/* 精细化声明 */
RDecl* elab_decl(Elaborator* elab, Decl* decl, uint32_t* out_count) {
    switch (decl->kind) {
        case DECL_VAR:
            elab_state_register_global(&elab->state, decl->var_decl.name, "global");
            return elab_var_decl(elab, decl, out_count);
        case DECL_FUN:
            elab_state_register_global(&elab->state, decl->fun_decl.name, "global");
            return elab_fun_decl(elab, decl, out_count);
        case DECL_INDUCTIVE:
            return elab_inductive_decl(elab, decl, out_count);
        case DECL_PRODUCT:
            return elab_product_decl(elab, decl, out_count);
        case DECL_AXIOM:
            elab_state_register_global(&elab->state, decl->axiom_decl.name, "global");
            return elab_axiom_decl(elab, decl, out_count);
        case DECL_EXAMPLE:
            return elab_example_decl(elab, decl, out_count);
        case DECL_EQUATION:
            elab_state_register_global(&elab->state, decl->var_decl.name, "global");
            return elab_equation_decl(elab, decl, out_count);
        default:
            *out_count = 0;
            return NULL;
    }
}

/* 精细化整个程序 */
RDecl* elab_program(Elaborator* elab, Decl** decls, uint32_t decl_count, uint32_t* out_count) {
    RDecl* all_rdecls = NULL;
    uint32_t total_count = 0;
    
    for (uint32_t i = 0; i < decl_count; i++) {
        uint32_t count = 0;
        RDecl* rdecls = elab_decl(elab, decls[i], &count);
        if (count > 0) {
            all_rdecls = (RDecl*)realloc(all_rdecls, (total_count + count) * sizeof(RDecl));
            memcpy(all_rdecls + total_count, rdecls, count * sizeof(RDecl));
            total_count += count;
            free(rdecls);
        }
    }
    
    *out_count = total_count;
    return all_rdecls;
}
