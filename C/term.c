#include "term.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * Term 构造函数
 * ============================================================ */

Term* term_new_var(uint32_t index, const char* name) {
    Term* t = (Term*)malloc(sizeof(Term));
    assert(t != NULL);
    t->kind = TERM_VAR;
    t->var.index = index;
    t->var.name = name;
    return t;
}

Term* term_new_global(const char* name) {
    Term* t = (Term*)malloc(sizeof(Term));
    assert(t != NULL);
    t->kind = TERM_GLOBAL;
    t->global = name;
    return t;
}

Term* term_new_type(void) {
    /* Type 是单例，可以复用 */
    static Term type_singleton = { .kind = TERM_TYPE };
    return &type_singleton;
}

Term* term_new_pi(const char* name, const Term* domain, const Term* codomain) {
    Term* t = (Term*)malloc(sizeof(Term));
    assert(t != NULL);
    t->kind = TERM_PI;
    t->pi.name = name;
    t->pi.domain = domain;
    t->pi.codomain = codomain;
    return t;
}

Term* term_new_lam(const char* name, const Term* param_type, const Term* body) {
    Term* t = (Term*)malloc(sizeof(Term));
    assert(t != NULL);
    t->kind = TERM_LAM;
    t->lam.name = name;
    t->lam.param_type = param_type;
    t->lam.body = body;
    return t;
}

Term* term_new_app(const Term* func, const Term* arg) {
    Term* t = (Term*)malloc(sizeof(Term));
    assert(t != NULL);
    t->kind = TERM_APP;
    t->app.func = func;
    t->app.arg = arg;
    return t;
}

/* ============================================================
 * Term 辅助函数
 * ============================================================ */

AppChain term_unfold_app(const Term* term) {
    if (term == NULL) {
        AppChain chain = { .head = NULL, .args = NULL, .count = 0 };
        return chain;
    }
    
    /* 先计算链长度 */
    size_t count = 0;
    const Term* current = term;
    while (current->kind == TERM_APP) {
        count++;
        current = current->app.func;
    }
    
    /* 分配 args 数组 */
    const Term** args = NULL;
    if (count > 0) {
        args = (const Term**)malloc(count * sizeof(const Term*));
        assert(args != NULL);
    }
    
    /* 填充 args（从右到左） */
    current = term;
    for (size_t i = count; i > 0; i--) {
        args[i - 1] = current->app.arg;
        current = current->app.func;
    }
    
    AppChain chain = { .head = current, .args = args, .count = count };
    return chain;
}

Term* term_mk_apps(const Term* head, const Term** args, size_t count) {
    if (count == 0 || args == NULL) {
        return (Term*)head;  /* 去除 const，因为调用者可能需要释放 */
    }
    
    const Term* result = head;
    for (size_t i = 0; i < count; i++) {
        result = term_new_app(result, args[i]);
    }
    return (Term*)result;
}

void app_chain_free(AppChain* chain) {
    if (chain->args != NULL) {
        free(chain->args);
        chain->args = NULL;
    }
    chain->count = 0;
}
