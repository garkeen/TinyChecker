#include "core_ops.h"
#include <assert.h>

/* ============================================================
 * shift - de Bruijn index 偏移
 * ============================================================ */

Term* term_shift(const Term* term, uint32_t amount, uint32_t cutoff) {
    /* 快速路径：不移动 */
    if (amount == 0) {
        return (Term*)term;
    }
    
    switch (term->kind) {
        case TERM_VAR:
            if (term->var.index >= cutoff) {
                return term_new_var(term->var.index + amount, term->var.name);
            }
            return (Term*)term;
        
        case TERM_GLOBAL:
        case TERM_TYPE:
            return (Term*)term;
        
        case TERM_PI: {
            Term* new_domain = term_shift(term->pi.domain, amount, cutoff);
            Term* new_codomain = term_shift(term->pi.codomain, amount, cutoff + 1);
            /* 不变时复用原节点 */
            if (new_domain == term->pi.domain && new_codomain == term->pi.codomain) {
                return (Term*)term;
            }
            return term_new_pi(term->pi.name, new_domain, new_codomain);
        }
        
        case TERM_LAM: {
            Term* new_param_type = term_shift(term->lam.param_type, amount, cutoff);
            Term* new_body = term_shift(term->lam.body, amount, cutoff + 1);
            if (new_param_type == term->lam.param_type && new_body == term->lam.body) {
                return (Term*)term;
            }
            return term_new_lam(term->lam.name, new_param_type, new_body);
        }
        
        case TERM_APP: {
            Term* new_func = term_shift(term->app.func, amount, cutoff);
            Term* new_arg = term_shift(term->app.arg, amount, cutoff);
            if (new_func == term->app.func && new_arg == term->app.arg) {
                return (Term*)term;
            }
            return term_new_app(new_func, new_arg);
        }
    }
    
    assert(0 && "unreachable");
    return NULL;
}

/* ============================================================
 * subst - 变量替换（带 cutoff）
 * ============================================================ */

static Term* do_subst(const Term* term, uint32_t index, const Term* replacement, uint32_t cutoff) {
    if (term == NULL) return NULL;
    
    switch (term->kind) {
        case TERM_VAR: {
            uint32_t target = index + cutoff;
            if (term->var.index == target) {
                return term_shift(replacement, cutoff, 0);
            }
            if (term->var.index > target) {
                return term_new_var(term->var.index - 1, term->var.name);
            }
            return (Term*)term;
        }
        
        case TERM_GLOBAL:
        case TERM_TYPE:
            return (Term*)term;
        
        case TERM_PI: {
            Term* new_domain = do_subst(term->pi.domain, index, replacement, cutoff);
            Term* new_codomain = do_subst(term->pi.codomain, index, replacement, cutoff + 1);
            if (new_domain == term->pi.domain && new_codomain == term->pi.codomain) {
                return (Term*)term;
            }
            return term_new_pi(term->pi.name, new_domain, new_codomain);
        }
        
        case TERM_LAM: {
            Term* new_param_type = do_subst(term->lam.param_type, index, replacement, cutoff);
            Term* new_body = do_subst(term->lam.body, index, replacement, cutoff + 1);
            if (new_param_type == term->lam.param_type && new_body == term->lam.body) {
                return (Term*)term;
            }
            return term_new_lam(term->lam.name, new_param_type, new_body);
        }
        
        case TERM_APP: {
            Term* new_func = do_subst(term->app.func, index, replacement, cutoff);
            Term* new_arg = do_subst(term->app.arg, index, replacement, cutoff);
            if (new_func == term->app.func && new_arg == term->app.arg) {
                return (Term*)term;
            }
            return term_new_app(new_func, new_arg);
        }
    }
    
    assert(0 && "unreachable");
    return NULL;
}

Term* term_subst(const Term* term, uint32_t index, const Term* replacement) {
    return do_subst(term, index, replacement, 0);
}

/* ============================================================
 * instantiate - beta 归约一步
 * ============================================================ */

Term* term_instantiate(const Term* body, const Term* arg) {
    if (body == NULL || arg == NULL) {
        return NULL;
    }
    
    /* 快速路径：body 就是变量 0 */
    if (body->kind == TERM_VAR && body->var.index == 0) {
        return (Term*)arg;
    }
    return term_subst(body, 0, arg);
}

/* ============================================================
 * instantiate_many - 批量实例化
 * ============================================================ */

Term* term_instantiate_many(const Term* body, const Term** args, size_t count) {
    if (count == 0 || args == NULL) {
        return (Term*)body;
    }
    
    const Term* current = body;
    for (size_t i = count; i > 0; i--) {
        current = term_instantiate(current, args[i - 1]);
    }
    return (Term*)current;
}

/* ============================================================
 * instantiate_env - 环境实例化
 * ============================================================ */

Term* term_instantiate_env(const Term* term, const Term** env, size_t env_size, uint32_t depth) {
    if (env_size == 0 || env == NULL) {
        return (Term*)term;
    }
    
    switch (term->kind) {
        case TERM_VAR:
            if (term->var.index < depth) {
                return (Term*)term;
            }
            {
                uint32_t offset = term->var.index - depth;
                if (offset < env_size) {
                    return term_shift(env[offset], depth, 0);
                }
                return term_new_var(term->var.index - env_size, term->var.name);
            }
        
        case TERM_GLOBAL:
        case TERM_TYPE:
            return (Term*)term;
        
        case TERM_PI: {
            const Term* new_domain = term_instantiate_env(term->pi.domain, env, env_size, depth);
            const Term* new_codomain = term_instantiate_env(term->pi.codomain, env, env_size, depth + 1);
            if (new_domain == term->pi.domain && new_codomain == term->pi.codomain) {
                return (Term*)term;
            }
            return term_new_pi(term->pi.name, new_domain, new_codomain);
        }
        
        case TERM_LAM: {
            const Term* new_param_type = term_instantiate_env(term->lam.param_type, env, env_size, depth);
            const Term* new_body = term_instantiate_env(term->lam.body, env, env_size, depth + 1);
            if (new_param_type == term->lam.param_type && new_body == term->lam.body) {
                return (Term*)term;
            }
            return term_new_lam(term->lam.name, new_param_type, new_body);
        }
        
        case TERM_APP: {
            const Term* new_func = term_instantiate_env(term->app.func, env, env_size, depth);
            const Term* new_arg = term_instantiate_env(term->app.arg, env, env_size, depth);
            if (new_func == term->app.func && new_arg == term->app.arg) {
                return (Term*)term;
            }
            return term_new_app(new_func, new_arg);
        }
    }
    
    assert(0 && "unreachable");
    return NULL;
}
