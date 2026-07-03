#include "restricted.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * lookup_local
 * ============================================================ */

int lookup_local(const char** local, uint32_t count, const char* name) {
    for (int i = (int)count - 1; i >= 0; i--) {
        if (strcmp(local[i], name) == 0) {
            return (int)count - 1 - i;
        }
    }
    return -1;
}

/* ============================================================
 * fold_pi_expr / fold_lam_expr
 * ============================================================ */

Expr* fold_pi_expr(char** names, Expr** types, uint32_t count, Expr* body) {
    Expr* result = body;
    for (int i = (int)count - 1; i >= 0; i--) {
        result = expr_new_pi(names[i], types[i], result);
    }
    return result;
}

Expr* fold_lam_expr(char** names, Expr** types, uint32_t count, Expr* body) {
    Expr* result = body;
    for (int i = (int)count - 1; i >= 0; i--) {
        Param* p = param_new(names[i], types[i]);
        result = expr_new_lambda(p, result);
    }
    return result;
}

/* ============================================================
 * split_pi_expr
 * ============================================================ */

PiTelescope split_pi_expr(Expr* expr) {
    PiTelescope tel = { .names = NULL, .types = NULL, .count = 0, .body = expr };
    
    /* 先计算数量（处理 Pi 和 Arrow） */
    Expr* current = expr;
    uint32_t count = 0;
    while (current != NULL) {
        if (current->kind == EXPR_PI) {
            if (current->pi.name == NULL) break;
            count++;
            current = current->pi.codomain;
        } else if (current->kind == EXPR_ARROW) {
            count++;
            current = current->arrow.codomain;
        } else {
            break;
        }
    }
    
    if (count == 0) {
        tel.body = expr;
        return tel;
    }
    
    /* 分配数组 */
    tel.names = (char**)malloc(count * sizeof(char*));
    tel.types = (Expr**)malloc(count * sizeof(Expr*));
    tel.count = count;
    
    /* 填充（处理 Pi 和 Arrow） */
    current = expr;
    for (uint32_t i = 0; i < count; i++) {
        if (current->kind == EXPR_PI) {
            tel.names[i] = strdup(current->pi.name);
            tel.types[i] = current->pi.domain;
            current = current->pi.codomain;
        } else if (current->kind == EXPR_ARROW) {
            tel.names[i] = strdup("_");
            tel.types[i] = current->arrow.domain;
            current = current->arrow.codomain;
        }
    }
    tel.body = current;
    
    return tel;
}

void pi_telescope_free(PiTelescope* tel) {
    if (tel->names) {
        free(tel->names);
        tel->names = NULL;
    }
    if (tel->types) {
        free(tel->types);
        tel->types = NULL;
    }
    tel->count = 0;
}

/* ============================================================
 * contains_atom
 * ============================================================ */

bool contains_atom(Expr* expr, const char* text) {
    if (expr == NULL) return false;
    
    switch (expr->kind) {
        case EXPR_ATOM:
            return expr->atom.text != NULL && strcmp(expr->atom.text, text) == 0;
        case EXPR_APP:
            return contains_atom(expr->app.func, text) || contains_atom(expr->app.arg, text);
        case EXPR_PI:
            return contains_atom(expr->pi.domain, text) || contains_atom(expr->pi.codomain, text);
        case EXPR_LAMBDA:
            return contains_atom(expr->lambda.param->typ, text) || contains_atom(expr->lambda.body, text);
        case EXPR_ARROW:
            return contains_atom(expr->arrow.domain, text) || contains_atom(expr->arrow.codomain, text);
        case EXPR_LET:
            return contains_atom(expr->let.typ, text) || 
                   contains_atom(expr->let.value, text) || 
                   contains_atom(expr->let.body, text);
        case EXPR_EQ:
            return contains_atom(expr->eq.typ, text) || 
                   contains_atom(expr->eq.lhs, text) || 
                   contains_atom(expr->eq.rhs, text);
        default:
            return false;
    }
}

/* ============================================================
 * free_vars
 * ============================================================ */

static void free_vars_collect(Expr* expr, FreeVarSet* set) {
    if (expr == NULL) return;
    
    switch (expr->kind) {
        case EXPR_ATOM: {
            if (expr->atom.text == NULL) return;
            if (strcmp(expr->atom.text, "Type") == 0 || strcmp(expr->atom.text, "Prop") == 0) {
                return;
            }
            /* 检查是否已存在 */
            for (uint32_t i = 0; i < set->count; i++) {
                if (strcmp(set->names[i], expr->atom.text) == 0) return;
            }
            /* 添加 */
            set->names = (char**)realloc(set->names, (set->count + 1) * sizeof(char*));
            set->names[set->count] = strdup(expr->atom.text);
            set->count++;
            return;
        }
        case EXPR_APP:
            free_vars_collect(expr->app.func, set);
            free_vars_collect(expr->app.arg, set);
            return;
        case EXPR_PI:
            free_vars_collect(expr->pi.domain, set);
            free_vars_collect(expr->pi.codomain, set);
            /* 移除绑定名 */
            if (expr->pi.name != NULL) {
                for (uint32_t i = 0; i < set->count; i++) {
                    if (strcmp(set->names[i], expr->pi.name) == 0) {
                        free(set->names[i]);
                        set->names[i] = set->names[set->count - 1];
                        set->count--;
                        break;
                    }
                }
            }
            return;
        case EXPR_LAMBDA:
            free_vars_collect(expr->lambda.param->typ, set);
            free_vars_collect(expr->lambda.body, set);
            /* 移除绑定名 */
            if (expr->lambda.param->name != NULL) {
                for (uint32_t i = 0; i < set->count; i++) {
                    if (strcmp(set->names[i], expr->lambda.param->name) == 0) {
                        free(set->names[i]);
                        set->names[i] = set->names[set->count - 1];
                        set->count--;
                        break;
                    }
                }
            }
            return;
        case EXPR_ARROW:
            free_vars_collect(expr->arrow.domain, set);
            free_vars_collect(expr->arrow.codomain, set);
            return;
        case EXPR_LET:
            free_vars_collect(expr->let.typ, set);
            free_vars_collect(expr->let.value, set);
            free_vars_collect(expr->let.body, set);
            if (expr->let.name != NULL) {
                for (uint32_t i = 0; i < set->count; i++) {
                    if (strcmp(set->names[i], expr->let.name) == 0) {
                        free(set->names[i]);
                        set->names[i] = set->names[set->count - 1];
                        set->count--;
                        break;
                    }
                }
            }
            return;
        case EXPR_EQ:
            free_vars_collect(expr->eq.typ, set);
            free_vars_collect(expr->eq.lhs, set);
            free_vars_collect(expr->eq.rhs, set);
            return;
        default:
            return;
    }
}

FreeVarSet free_vars(Expr* expr) {
    FreeVarSet set = { .names = NULL, .count = 0 };
    free_vars_collect(expr, &set);
    return set;
}

void free_var_set_free(FreeVarSet* set) {
    for (uint32_t i = 0; i < set->count; i++) {
        free(set->names[i]);
    }
    free(set->names);
    set->names = NULL;
    set->count = 0;
}

/* ============================================================
 * rename_bound_occurrences (内部辅助)
 * ============================================================ */

static Expr* rename_bound(Expr* expr, const char* old_name, const char* new_name) {
    if (expr == NULL) return NULL;
    
    switch (expr->kind) {
        case EXPR_ATOM:
            if (expr->atom.text != NULL && strcmp(expr->atom.text, old_name) == 0) {
                return expr_new_atom(strdup(new_name));
            }
            return expr;  /* 不变，返回原指针 */
        
        case EXPR_APP: {
            Expr* new_func = rename_bound(expr->app.func, old_name, new_name);
            Expr* new_arg = rename_bound(expr->app.arg, old_name, new_name);
            if (new_func == expr->app.func && new_arg == expr->app.arg) return expr;
            return expr_new_app(new_func, new_arg);
        }
        
        case EXPR_PI: {
            Expr* new_domain = rename_bound(expr->pi.domain, old_name, new_name);
            if (expr->pi.name != NULL && strcmp(expr->pi.name, old_name) == 0) {
                /* 绑定名匹配，不重命名 body */
                if (new_domain == expr->pi.domain) return expr;
                return expr_new_pi(expr->pi.name, new_domain, expr->pi.codomain);
            }
            Expr* new_codomain = rename_bound(expr->pi.codomain, old_name, new_name);
            if (new_domain == expr->pi.domain && new_codomain == expr->pi.codomain) return expr;
            return expr_new_pi(expr->pi.name, new_domain, new_codomain);
        }
        
        case EXPR_LAMBDA: {
            Expr* new_param_type = rename_bound(expr->lambda.param->typ, old_name, new_name);
            if (expr->lambda.param->name != NULL && strcmp(expr->lambda.param->name, old_name) == 0) {
                if (new_param_type == expr->lambda.param->typ) return expr;
                Param* p = param_new(expr->lambda.param->name, new_param_type);
                return expr_new_lambda(p, expr->lambda.body);
            }
            Expr* new_body = rename_bound(expr->lambda.body, old_name, new_name);
            if (new_param_type == expr->lambda.param->typ && new_body == expr->lambda.body) return expr;
            Param* p = param_new(expr->lambda.param->name, new_param_type);
            return expr_new_lambda(p, new_body);
        }
        
        case EXPR_ARROW: {
            Expr* new_domain = rename_bound(expr->arrow.domain, old_name, new_name);
            Expr* new_codomain = rename_bound(expr->arrow.codomain, old_name, new_name);
            if (new_domain == expr->arrow.domain && new_codomain == expr->arrow.codomain) return expr;
            return expr_new_arrow(new_domain, new_codomain);
        }
        
        case EXPR_LET: {
            Expr* new_typ = rename_bound(expr->let.typ, old_name, new_name);
            Expr* new_value = rename_bound(expr->let.value, old_name, new_name);
            if (expr->let.name != NULL && strcmp(expr->let.name, old_name) == 0) {
                if (new_typ == expr->let.typ && new_value == expr->let.value) return expr;
                return expr_new_let(expr->let.name, new_typ, new_value, expr->let.body);
            }
            Expr* new_body = rename_bound(expr->let.body, old_name, new_name);
            if (new_typ == expr->let.typ && new_value == expr->let.value && new_body == expr->let.body) return expr;
            return expr_new_let(expr->let.name, new_typ, new_value, new_body);
        }
        
        case EXPR_EQ: {
            Expr* new_typ = rename_bound(expr->eq.typ, old_name, new_name);
            Expr* new_lhs = rename_bound(expr->eq.lhs, old_name, new_name);
            Expr* new_rhs = rename_bound(expr->eq.rhs, old_name, new_name);
            if (new_typ == expr->eq.typ && new_lhs == expr->eq.lhs && new_rhs == expr->eq.rhs) return expr;
            return expr_new_eq(new_typ, new_lhs, new_rhs);
        }
        
        default:
            return expr;
    }
}

/* 默认的 fresh_name 函数 */
static char* default_fresh_name(const char* prefix, void* ctx) {
    (void)ctx;
    static uint32_t counter = 0;
    counter++;
    char* name = (char*)malloc(32);
    snprintf(name, 32, "_%s%u", prefix ? prefix : "x", counter);
    return name;
}

/* ============================================================
 * substitute_expr
 * ============================================================ */

static Expr* do_substitute(Expr* expr, 
                           const char** keys, Expr** values, uint32_t mapping_count,
                           FreshNameFn fresh_name, void* fresh_ctx) {
    if (expr == NULL || mapping_count == 0) return expr;
    
    /* 使用默认的 fresh_name 函数 */
    if (fresh_name == NULL) {
        fresh_name = default_fresh_name;
        fresh_ctx = NULL;
    }
    
    switch (expr->kind) {
        case EXPR_ATOM: {
            if (expr->atom.text == NULL) return expr;
            for (uint32_t i = 0; i < mapping_count; i++) {
                if (strcmp(keys[i], expr->atom.text) == 0) {
                    return values[i];
                }
            }
            return expr;
        }
        
        case EXPR_APP: {
            Expr* new_func = do_substitute(expr->app.func, keys, values, mapping_count, fresh_name, fresh_ctx);
            Expr* new_arg = do_substitute(expr->app.arg, keys, values, mapping_count, fresh_name, fresh_ctx);
            if (new_func == expr->app.func && new_arg == expr->app.arg) return expr;
            return expr_new_app(new_func, new_arg);
        }
        
        case EXPR_PI: {
            Expr* new_domain = do_substitute(expr->pi.domain, keys, values, mapping_count, fresh_name, fresh_ctx);
            
            /* 构建 body_mapping：移除绑定名 */
            const char** body_keys = NULL;
            Expr** body_values = NULL;
            uint32_t body_count = 0;
            
            if (expr->pi.name != NULL) {
                body_keys = (const char**)malloc(mapping_count * sizeof(const char*));
                body_values = (Expr**)malloc(mapping_count * sizeof(Expr*));
                for (uint32_t i = 0; i < mapping_count; i++) {
                    if (strcmp(keys[i], expr->pi.name) != 0) {
                        body_keys[body_count] = keys[i];
                        body_values[body_count] = values[i];
                        body_count++;
                    }
                }
            } else {
                body_keys = keys;
                body_values = values;
                body_count = mapping_count;
            }
            
            /* 检查是否需要 alpha-renaming */
            char* binder = strdup(expr->pi.name ? expr->pi.name : "_");
            Expr* codomain = expr->pi.codomain;
            bool needs_rename = false;
            
            if (expr->pi.name != NULL && body_count > 0) {
                for (uint32_t i = 0; i < body_count; i++) {
                    FreeVarSet fv = free_vars(body_values[i]);
                    for (uint32_t j = 0; j < fv.count; j++) {
                        if (strcmp(fv.names[j], binder) == 0) {
                            needs_rename = true;
                            break;
                        }
                    }
                    free_var_set_free(&fv);
                    if (needs_rename) break;
                }
            }
            
            if (needs_rename) {
                char* fresh = fresh_name(binder, fresh_ctx);
                codomain = rename_bound(codomain, binder, fresh);
                free(binder);
                binder = fresh;
            }
            
            Expr* new_codomain = do_substitute(codomain, body_keys, body_values, body_count, fresh_name, fresh_ctx);
            
            if (expr->pi.name != NULL) {
                free(body_keys);
                free(body_values);
            }
            
            if (new_domain == expr->pi.domain && new_codomain == codomain && strcmp(binder, expr->pi.name ? expr->pi.name : "_") == 0) {
                free(binder);
                return expr;
            }
            return expr_new_pi(binder, new_domain, new_codomain);
        }
        
        case EXPR_LAMBDA: {
            Expr* new_param_type = do_substitute(expr->lambda.param->typ, keys, values, mapping_count, fresh_name, fresh_ctx);
            
            /* 构建 body_mapping */
            const char** body_keys = NULL;
            Expr** body_values = NULL;
            uint32_t body_count = 0;
            
            if (expr->lambda.param->name != NULL) {
                body_keys = (const char**)malloc(mapping_count * sizeof(const char*));
                body_values = (Expr**)malloc(mapping_count * sizeof(Expr*));
                for (uint32_t i = 0; i < mapping_count; i++) {
                    if (strcmp(keys[i], expr->lambda.param->name) != 0) {
                        body_keys[body_count] = keys[i];
                        body_values[body_count] = values[i];
                        body_count++;
                    }
                }
            } else {
                body_keys = keys;
                body_values = values;
                body_count = mapping_count;
            }
            
            /* 检查是否需要 alpha-renaming */
            char* binder = strdup(expr->lambda.param->name ? expr->lambda.param->name : "_");
            Expr* body = expr->lambda.body;
            bool needs_rename = false;
            
            if (expr->lambda.param->name != NULL && body_count > 0) {
                for (uint32_t i = 0; i < body_count; i++) {
                    FreeVarSet fv = free_vars(body_values[i]);
                    for (uint32_t j = 0; j < fv.count; j++) {
                        if (strcmp(fv.names[j], binder) == 0) {
                            needs_rename = true;
                            break;
                        }
                    }
                    free_var_set_free(&fv);
                    if (needs_rename) break;
                }
            }
            
            if (needs_rename) {
                char* fresh = fresh_name(binder, fresh_ctx);
                body = rename_bound(body, binder, fresh);
                free(binder);
                binder = fresh;
            }
            
            Expr* new_body = do_substitute(body, body_keys, body_values, body_count, fresh_name, fresh_ctx);
            
            if (expr->lambda.param->name != NULL) {
                free(body_keys);
                free(body_values);
            }
            
            if (new_param_type == expr->lambda.param->typ && new_body == body && strcmp(binder, expr->lambda.param->name ? expr->lambda.param->name : "_") == 0) {
                free(binder);
                return expr;
            }
            Param* p = param_new(binder, new_param_type);
            return expr_new_lambda(p, new_body);
        }
        
        case EXPR_ARROW: {
            Expr* new_domain = do_substitute(expr->arrow.domain, keys, values, mapping_count, fresh_name, fresh_ctx);
            Expr* new_codomain = do_substitute(expr->arrow.codomain, keys, values, mapping_count, fresh_name, fresh_ctx);
            if (new_domain == expr->arrow.domain && new_codomain == expr->arrow.codomain) return expr;
            return expr_new_arrow(new_domain, new_codomain);
        }
        
        case EXPR_LET: {
            Expr* new_typ = do_substitute(expr->let.typ, keys, values, mapping_count, fresh_name, fresh_ctx);
            Expr* new_value = do_substitute(expr->let.value, keys, values, mapping_count, fresh_name, fresh_ctx);
            
            /* 构建 body_mapping */
            const char** body_keys = NULL;
            Expr** body_values = NULL;
            uint32_t body_count = 0;
            
            if (expr->let.name != NULL) {
                body_keys = (const char**)malloc(mapping_count * sizeof(const char*));
                body_values = (Expr**)malloc(mapping_count * sizeof(Expr*));
                for (uint32_t i = 0; i < mapping_count; i++) {
                    if (strcmp(keys[i], expr->let.name) != 0) {
                        body_keys[body_count] = keys[i];
                        body_values[body_count] = values[i];
                        body_count++;
                    }
                }
            } else {
                body_keys = keys;
                body_values = values;
                body_count = mapping_count;
            }
            
            Expr* new_body = do_substitute(expr->let.body, body_keys, body_values, body_count, fresh_name, fresh_ctx);
            
            if (expr->let.name != NULL) {
                free(body_keys);
                free(body_values);
            }
            
            if (new_typ == expr->let.typ && new_value == expr->let.value && new_body == expr->let.body) return expr;
            return expr_new_let(expr->let.name, new_typ, new_value, new_body);
        }
        
        case EXPR_EQ: {
            Expr* new_typ = do_substitute(expr->eq.typ, keys, values, mapping_count, fresh_name, fresh_ctx);
            Expr* new_lhs = do_substitute(expr->eq.lhs, keys, values, mapping_count, fresh_name, fresh_ctx);
            Expr* new_rhs = do_substitute(expr->eq.rhs, keys, values, mapping_count, fresh_name, fresh_ctx);
            if (new_typ == expr->eq.typ && new_lhs == expr->eq.lhs && new_rhs == expr->eq.rhs) return expr;
            return expr_new_eq(new_typ, new_lhs, new_rhs);
        }
        
        default:
            return expr;
    }
}

Expr* substitute_expr(Expr* expr, 
                      const char** keys, Expr** values, uint32_t mapping_count,
                      FreshNameFn fresh_name, void* fresh_ctx) {
    return do_substitute(expr, keys, values, mapping_count, fresh_name, fresh_ctx);
}

/* ============================================================
 * restricted_expr_to_core
 * ============================================================ */

Term* restricted_expr_to_core(Expr* expr, const char** local, uint32_t local_count) {
    if (expr == NULL) return NULL;
    
    switch (expr->kind) {
        case EXPR_ATOM: {
            if (expr->atom.text == NULL) return term_new_type();
            if (strcmp(expr->atom.text, "Type") == 0 || strcmp(expr->atom.text, "Prop") == 0) {
                return term_new_type();
            }
            int index = lookup_local(local, local_count, expr->atom.text);
            if (index >= 0) {
                return term_new_var((uint32_t)index, expr->atom.text);
            }
            return term_new_global(expr->atom.text);
        }
        
        case EXPR_APP: {
            Term* func = restricted_expr_to_core(expr->app.func, local, local_count);
            Term* arg = restricted_expr_to_core(expr->app.arg, local, local_count);
            return term_new_app(func, arg);
        }
        
        case EXPR_PI: {
            Term* domain = restricted_expr_to_core(expr->pi.domain, local, local_count);
            /* 扩展 local */
            const char** new_local = (const char**)malloc((local_count + 1) * sizeof(const char*));
            memcpy(new_local, local, local_count * sizeof(const char*));
            new_local[local_count] = expr->pi.name;
            Term* codomain = restricted_expr_to_core(expr->pi.codomain, new_local, local_count + 1);
            free(new_local);
            return term_new_pi(expr->pi.name, domain, codomain);
        }
        
        case EXPR_LAMBDA: {
            Term* param_type = restricted_expr_to_core(expr->lambda.param->typ, local, local_count);
            const char** new_local = (const char**)malloc((local_count + 1) * sizeof(const char*));
            memcpy(new_local, local, local_count * sizeof(const char*));
            new_local[local_count] = expr->lambda.param->name;
            Term* body = restricted_expr_to_core(expr->lambda.body, new_local, local_count + 1);
            free(new_local);
            return term_new_lam(expr->lambda.param->name, param_type, body);
        }
        
        case EXPR_ARROW: {
            Term* domain = restricted_expr_to_core(expr->arrow.domain, local, local_count);
            /* 匿名参数，用 "_" */
            const char** new_local = (const char**)malloc((local_count + 1) * sizeof(const char*));
            memcpy(new_local, local, local_count * sizeof(const char*));
            new_local[local_count] = "_";
            Term* codomain = restricted_expr_to_core(expr->arrow.codomain, new_local, local_count + 1);
            free(new_local);
            return term_new_pi("_", domain, codomain);
        }
        
        case EXPR_LET: {
            /* let x: T = e in body  =>  (\x: T. body) e */
            Term* typ = restricted_expr_to_core(expr->let.typ, local, local_count);
            Term* value = restricted_expr_to_core(expr->let.value, local, local_count);
            const char** new_local = (const char**)malloc((local_count + 1) * sizeof(const char*));
            memcpy(new_local, local, local_count * sizeof(const char*));
            new_local[local_count] = expr->let.name;
            Term* body = restricted_expr_to_core(expr->let.body, new_local, local_count + 1);
            free(new_local);
            Term* lam = term_new_lam(expr->let.name, typ, body);
            return term_new_app(lam, value);
        }
        
        case EXPR_EQ: {
            /* [T] a == b  =>  Eq T a b */
            Term* typ = restricted_expr_to_core(expr->eq.typ, local, local_count);
            Term* lhs = restricted_expr_to_core(expr->eq.lhs, local, local_count);
            Term* rhs = restricted_expr_to_core(expr->eq.rhs, local, local_count);
            Term* eq = term_new_global("Eq");
            eq = term_new_app(eq, typ);
            eq = term_new_app(eq, lhs);
            eq = term_new_app(eq, rhs);
            return eq;
        }
        
        default:
            return term_new_type();  /* 占位 */
    }
}

/* ============================================================
 * final_result_of_ctor_type
 * ============================================================ */

Expr* final_result_of_ctor_type(Expr* expr) {
    Expr* current = expr;
    while (current != NULL && current->kind == EXPR_PI) {
        current = current->pi.codomain;
    }
    return current;
}

/* ============================================================
 * restricted_decl_to_core - 受限声明到核心声明的转换
 * ============================================================ */

CDecl* restricted_decl_to_core(RDecl* rdecl, uint32_t* out_count) {
    if (rdecl == NULL) {
        *out_count = 0;
        return NULL;
    }
    
    CDecl* cdecl = (CDecl*)calloc(1, sizeof(CDecl));
    
    switch (rdecl->kind) {
        case RDECL_DEFINITION: {
            cdecl->kind = CDECL_DEFINITION;
            cdecl->def.name = strdup(rdecl->def.name);
            cdecl->def.typ = restricted_expr_to_core(rdecl->def.typ, NULL, 0);
            cdecl->def.value = rdecl->def.value ? restricted_expr_to_core(rdecl->def.value, NULL, 0) : NULL;
            cdecl->def.kind = strdup(rdecl->def.kind);
            *out_count = 1;
            return cdecl;
        }
        
        case RDECL_TYPE_CTOR: {
            cdecl->kind = CDECL_TYPE_CTOR;
            cdecl->type_ctor.name = strdup(rdecl->type_ctor.name);
            cdecl->type_ctor.kind = strdup(rdecl->type_ctor.kind);
            cdecl->type_ctor.family_kind = strdup(rdecl->type_ctor.family_kind);
            
            /* param_telescope */
            cdecl->type_ctor.param_count = rdecl->type_ctor.param_count;
            if (cdecl->type_ctor.param_count > 0) {
                cdecl->type_ctor.param_names = (char**)malloc(cdecl->type_ctor.param_count * sizeof(char*));
                cdecl->type_ctor.param_types = (Term**)malloc(cdecl->type_ctor.param_count * sizeof(Term*));
                for (uint32_t i = 0; i < cdecl->type_ctor.param_count; i++) {
                    cdecl->type_ctor.param_names[i] = strdup(rdecl->type_ctor.param_names[i]);
                    /* 参数类型需要在正确的上下文中转换 */
                    const char** local = (const char**)malloc(i * sizeof(const char*));
                    for (uint32_t j = 0; j < i; j++) {
                        local[j] = rdecl->type_ctor.param_names[j];
                    }
                    cdecl->type_ctor.param_types[i] = restricted_expr_to_core(rdecl->type_ctor.param_types[i], local, i);
                    free(local);
                }
            }
            
            /* index_telescope */
            cdecl->type_ctor.index_count = rdecl->type_ctor.index_count;
            if (cdecl->type_ctor.index_count > 0) {
                cdecl->type_ctor.index_names = (char**)malloc(cdecl->type_ctor.index_count * sizeof(char*));
                cdecl->type_ctor.index_types = (Term**)malloc(cdecl->type_ctor.index_count * sizeof(Term*));
                for (uint32_t i = 0; i < cdecl->type_ctor.index_count; i++) {
                    cdecl->type_ctor.index_names[i] = strdup(rdecl->type_ctor.index_names[i]);
                    /* 索引类型在参数上下文中转换 */
                    const char** local = (const char**)malloc((cdecl->type_ctor.param_count + i) * sizeof(const char*));
                    for (uint32_t j = 0; j < cdecl->type_ctor.param_count; j++) {
                        local[j] = rdecl->type_ctor.param_names[j];
                    }
                    for (uint32_t j = 0; j < i; j++) {
                        local[cdecl->type_ctor.param_count + j] = rdecl->type_ctor.index_names[j];
                    }
                    cdecl->type_ctor.index_types[i] = restricted_expr_to_core(rdecl->type_ctor.index_types[i], local, cdecl->type_ctor.param_count + i);
                    free(local);
                }
            }
            
            /* typ */
            cdecl->type_ctor.typ = restricted_expr_to_core(rdecl->type_ctor.typ, NULL, 0);
            
            /* constructor_names */
            cdecl->type_ctor.constructor_count = rdecl->type_ctor.constructor_count;
            cdecl->type_ctor.constructor_names = (char**)malloc(cdecl->type_ctor.constructor_count * sizeof(char*));
            for (uint32_t i = 0; i < cdecl->type_ctor.constructor_count; i++) {
                cdecl->type_ctor.constructor_names[i] = strdup(rdecl->type_ctor.constructor_names[i]);
            }
            
            *out_count = 1;
            return cdecl;
        }
        
        case RDECL_DATA_CTOR: {
            cdecl->kind = CDECL_DATA_CTOR;
            cdecl->data_ctor.name = strdup(rdecl->data_ctor.name);
            cdecl->data_ctor.owner = strdup(rdecl->data_ctor.owner);
            cdecl->data_ctor.kind = strdup(rdecl->data_ctor.kind);
            
            /* typ */
            cdecl->data_ctor.typ = restricted_expr_to_core(rdecl->data_ctor.typ, NULL, 0);
            
            /* constructor_parameter_list */
            cdecl->data_ctor.param_count = rdecl->data_ctor.param_count;
            cdecl->data_ctor.spec_param_count = rdecl->data_ctor.spec_param_count;
            if (cdecl->data_ctor.param_count > 0) {
                cdecl->data_ctor.param_names = (char**)malloc(cdecl->data_ctor.param_count * sizeof(char*));
                cdecl->data_ctor.param_types = (Term**)malloc(cdecl->data_ctor.param_count * sizeof(Term*));
                /* 构建构造器环境名列表（从 typ 中提取） */
                PiTelescope ctor_tel = split_pi_expr(rdecl->data_ctor.typ);
                for (uint32_t i = 0; i < cdecl->data_ctor.param_count; i++) {
                    cdecl->data_ctor.param_names[i] = strdup(rdecl->data_ctor.param_names[i]);
                    /* 在构造器参数上下文中转换 param_types（与 Python 一致） */
                    cdecl->data_ctor.param_types[i] = restricted_expr_to_core(
                        rdecl->data_ctor.param_types[i],
                        (const char**)ctor_tel.names, i < ctor_tel.count ? i : ctor_tel.count);
                }
                pi_telescope_free(&ctor_tel);
            }
            
            /* target_args: spec_param_count + index_count 个元素 */
            /* 前 spec_param_count 个是类型参数变量，后面是 target_index_exprs */
            {
                PiTelescope ctor_tel = split_pi_expr(rdecl->data_ctor.typ);
                
                cdecl->data_ctor.target_arg_count = rdecl->data_ctor.spec_param_count + rdecl->data_ctor.target_index_count;
                if (cdecl->data_ctor.target_arg_count > 0) {
                    cdecl->data_ctor.target_args = (Term**)malloc(cdecl->data_ctor.target_arg_count * sizeof(Term*));
                    /* 前 spec_param_count 个：类型参数变量 */
                    for (uint32_t i = 0; i < rdecl->data_ctor.spec_param_count; i++) {
                        cdecl->data_ctor.target_args[i] = term_new_var(
                            ctor_tel.count - 1 - i, rdecl->data_ctor.param_names[i]);
                    }
                    /* 后 index_count 个：target_index_exprs 在构造器环境中转换 */
                    for (uint32_t i = 0; i < rdecl->data_ctor.target_index_count; i++) {
                        cdecl->data_ctor.target_args[rdecl->data_ctor.spec_param_count + i] = restricted_expr_to_core(
                            rdecl->data_ctor.target_index_exprs[i],
                            (const char**)ctor_tel.names, ctor_tel.count);
                    }
                }
                
                pi_telescope_free(&ctor_tel);
            }
            
            /* recursive_fields */
            cdecl->data_ctor.recursive_field_count = rdecl->data_ctor.recursive_field_count;
            if (cdecl->data_ctor.recursive_field_count > 0) {
                cdecl->data_ctor.recursive_fields = (CRecursiveFieldDecl*)calloc(cdecl->data_ctor.recursive_field_count, sizeof(CRecursiveFieldDecl));
                for (uint32_t i = 0; i < cdecl->data_ctor.recursive_field_count; i++) {
                    RRecFieldInfo* rrec = &rdecl->data_ctor.recursive_fields[i];
                    CRecursiveFieldDecl* crec = &cdecl->data_ctor.recursive_fields[i];
                    
                    crec->field_position = rrec->field_position;
                    
                    /* ho_telescope: 在构造器参数[:spec_param_count + field_position] + ho_names[:j] 环境中转换 */
                    crec->ho_count = rrec->ho_count;
                    if (crec->ho_count > 0) {
                        crec->ho_names = (char**)malloc(crec->ho_count * sizeof(char*));
                        crec->ho_types = (Term**)malloc(crec->ho_count * sizeof(Term*));
                        
                        PiTelescope ctor_tel = split_pi_expr(rdecl->data_ctor.typ);
                        
                        /* Python: ctor_env_names[: spec_param_count + field_position] + ho_names[:j] */
                        uint32_t ho_prefix = rdecl->data_ctor.spec_param_count + rrec->field_position;
                        if (ho_prefix > ctor_tel.count) ho_prefix = ctor_tel.count;
                        
                        for (uint32_t j = 0; j < crec->ho_count; j++) {
                            crec->ho_names[j] = strdup(rrec->ho_names[j]);
                            uint32_t ho_env_count = ho_prefix + j;
                            const char** ho_env = (const char**)malloc(ho_env_count * sizeof(const char*));
                            for (uint32_t k = 0; k < ho_prefix; k++) {
                                ho_env[k] = ctor_tel.names[k];
                            }
                            for (uint32_t k = 0; k < j; k++) {
                                ho_env[ho_prefix + k] = rrec->ho_names[k];
                            }
                            crec->ho_types[j] = restricted_expr_to_core(rrec->ho_types[j], ho_env, ho_env_count);
                            free(ho_env);
                        }
                        
                        pi_telescope_free(&ctor_tel);
                    }
                    
                    /* recursive_target_args: spec_param_count + index_count 个元素 */
                    crec->recursive_target_arg_count = rdecl->data_ctor.spec_param_count + rrec->target_index_count;
                    if (crec->recursive_target_arg_count > 0) {
                        PiTelescope ctor_tel = split_pi_expr(rdecl->data_ctor.typ);
                        
                        /* Python: ctor_env_names[: spec_param_count + field_position] + ho_names */
                        uint32_t env_prefix = rdecl->data_ctor.spec_param_count + rrec->field_position;
                        if (env_prefix > ctor_tel.count) env_prefix = ctor_tel.count;
                        uint32_t rec_env_count = env_prefix + rrec->ho_count;
                        const char** rec_env = (const char**)malloc(rec_env_count * sizeof(const char*));
                        for (uint32_t j = 0; j < env_prefix; j++) {
                            rec_env[j] = ctor_tel.names[j];
                        }
                        for (uint32_t j = 0; j < rrec->ho_count; j++) {
                            rec_env[env_prefix + j] = rrec->ho_names[j];
                        }
                        
                        crec->recursive_target_args = (Term**)malloc(crec->recursive_target_arg_count * sizeof(Term*));
                        /* 前 spec_param_count 个：类型参数变量 */
                        for (uint32_t j = 0; j < rdecl->data_ctor.spec_param_count; j++) {
                            crec->recursive_target_args[j] = term_new_var(
                                rec_env_count - 1 - j, rdecl->data_ctor.param_names[j]);
                        }
                        /* 后 index_count 个：target_index_exprs */
                        for (uint32_t j = 0; j < rrec->target_index_count; j++) {
                            crec->recursive_target_args[rdecl->data_ctor.spec_param_count + j] = restricted_expr_to_core(
                                rrec->target_index_exprs[j], rec_env, rec_env_count);
                        }
                        
                        free(rec_env);
                        pi_telescope_free(&ctor_tel);
                    }
                }
            }
            
            *out_count = 1;
            return cdecl;
        }
        
        case RDECL_ELIMINATOR: {
            cdecl->kind = CDECL_ELIMINATOR;
            cdecl->eliminator.name = strdup(rdecl->eliminator.name);
            cdecl->eliminator.owner = strdup(rdecl->eliminator.owner);
            cdecl->eliminator.kind = strdup(rdecl->eliminator.kind);
            
            /* motive_type 和 typ */
            cdecl->eliminator.motive_type = restricted_expr_to_core(rdecl->eliminator.motive_type, NULL, 0);
            cdecl->eliminator.typ = restricted_expr_to_core(rdecl->eliminator.typ, NULL, 0);
            
            /* branch_order */
            cdecl->eliminator.branch_count = rdecl->eliminator.branch_count;
            cdecl->eliminator.branch_order = (char**)malloc(cdecl->eliminator.branch_count * sizeof(char*));
            for (uint32_t i = 0; i < cdecl->eliminator.branch_count; i++) {
                cdecl->eliminator.branch_order[i] = strdup(rdecl->eliminator.branch_order[i]);
            }
            
            *out_count = 1;
            return cdecl;
        }
    }
    
    *out_count = 0;
    free(cdecl);
    return NULL;
}
