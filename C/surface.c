#include "surface.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint32_t next_surface_id = 1;

static uint32_t alloc_surface_id(void) {
    return next_surface_id++;
}

/* ============================================================
 * 表达式构造函数
 * ============================================================ */

Expr* expr_new_atom(const char* text) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_ATOM;
    e->surface_id = alloc_surface_id();
    e->atom.text = text;
    return e;
}

Expr* expr_new_arrow(Expr* domain, Expr* codomain) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_ARROW;
    e->surface_id = alloc_surface_id();
    e->arrow.domain = domain;
    e->arrow.codomain = codomain;
    return e;
}

Expr* expr_new_pi(const char* name, Expr* domain, Expr* codomain) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_PI;
    e->surface_id = alloc_surface_id();
    e->pi.name = name;
    e->pi.domain = domain;
    e->pi.codomain = codomain;
    return e;
}

Expr* expr_new_lambda(Param* param, Expr* body) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_LAMBDA;
    e->surface_id = alloc_surface_id();
    e->lambda.param = param;
    e->lambda.body = body;
    return e;
}

Expr* expr_new_app(Expr* func, Expr* arg) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_APP;
    e->surface_id = alloc_surface_id();
    e->app.func = func;
    e->app.arg = arg;
    return e;
}

Expr* expr_new_let(const char* name, Expr* typ, Expr* value, Expr* body) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_LET;
    e->surface_id = alloc_surface_id();
    e->let.name = name;
    e->let.typ = typ;
    e->let.value = value;
    e->let.body = body;
    return e;
}

Expr* expr_new_eq(Expr* typ, Expr* lhs, Expr* rhs) {
    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    assert(e != NULL);
    e->kind = EXPR_EQ;
    e->surface_id = alloc_surface_id();
    e->eq.typ = typ;
    e->eq.lhs = lhs;
    e->eq.rhs = rhs;
    return e;
}

/* ============================================================
 * 声明构造函数
 * ============================================================ */

Decl* decl_new_var(const char* kind, const char* name, Expr* typ, Expr* value) {
    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    assert(d != NULL);
    d->kind = DECL_VAR;
    d->surface_id = alloc_surface_id();
    d->var_decl.kind_str = kind;
    d->var_decl.name = name;
    d->var_decl.typ = typ;
    d->var_decl.value = value;
    return d;
}

Decl* decl_new_fun(const char* kind, const char* name, Param** params, uint32_t param_count, Expr* ret_type, Expr* body) {
    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    assert(d != NULL);
    d->kind = DECL_FUN;
    d->surface_id = alloc_surface_id();
    d->fun_decl.kind_str = kind;
    d->fun_decl.name = name;
    d->fun_decl.params = params;
    d->fun_decl.param_count = param_count;
    d->fun_decl.ret_type = ret_type;
    d->fun_decl.body = body;
    return d;
}

Decl* decl_new_inductive(const char* kind, const char* name, Param** params, uint32_t param_count, Expr* arity, CtorDecl** ctors, uint32_t ctor_count) {
    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    assert(d != NULL);
    d->kind = DECL_INDUCTIVE;
    d->surface_id = alloc_surface_id();
    d->inductive_decl.kind_str = kind;
    d->inductive_decl.name = name;
    d->inductive_decl.params = params;
    d->inductive_decl.param_count = param_count;
    d->inductive_decl.arity = arity;
    d->inductive_decl.ctors = ctors;
    d->inductive_decl.ctor_count = ctor_count;
    return d;
}

Decl* decl_new_axiom(const char* name, Expr* typ) {
    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    assert(d != NULL);
    d->kind = DECL_AXIOM;
    d->surface_id = alloc_surface_id();
    d->axiom_decl.name = name;
    d->axiom_decl.typ = typ;
    return d;
}

Param* param_new(const char* name, Expr* typ) {
    Param* p = (Param*)calloc(1, sizeof(Param));
    assert(p != NULL);
    p->name = name ? strdup(name) : NULL;
    p->typ = typ;
    return p;
}

CtorDecl* ctor_decl_new(const char* name, Expr* typ) {
    CtorDecl* c = (CtorDecl*)calloc(1, sizeof(CtorDecl));
    assert(c != NULL);
    c->name = name;
    c->typ = typ;
    return c;
}

/* ============================================================
 * 释放函数
 * ============================================================ */

void expr_free(Expr* expr) {
    if (expr == NULL) return;
    
    switch (expr->kind) {
        case EXPR_ATOM:
            /* 字符串不拥有，不释放 */
            break;
        case EXPR_ARROW:
            expr_free(expr->arrow.domain);
            expr_free(expr->arrow.codomain);
            break;
        case EXPR_PI:
            expr_free(expr->pi.domain);
            expr_free(expr->pi.codomain);
            break;
        case EXPR_LAMBDA:
            if (expr->lambda.param) {
                expr_free(expr->lambda.param->typ);
                free(expr->lambda.param);
            }
            expr_free(expr->lambda.body);
            break;
        case EXPR_APP:
            expr_free(expr->app.func);
            expr_free(expr->app.arg);
            break;
        case EXPR_LET:
            expr_free(expr->let.typ);
            expr_free(expr->let.value);
            expr_free(expr->let.body);
            break;
        case EXPR_EQ:
            expr_free(expr->eq.typ);
            expr_free(expr->eq.lhs);
            expr_free(expr->eq.rhs);
            break;
        case EXPR_MATCH:
            expr_free(expr->match.scrutinee);
            for (uint32_t i = 0; i < expr->match.family_arg_count; i++) {
                expr_free(expr->match.family_args[i]);
            }
            free(expr->match.family_args);
            free(expr->match.bind_names);
            expr_free(expr->match.motive_body);
            for (uint32_t i = 0; i < expr->match.branch_count; i++) {
                /* TODO: 释放分支 */
            }
            free(expr->match.branches);
            break;
        case EXPR_CASE:
            /* TODO: 释放 case */
            break;
        case EXPR_PRODUCT:
            for (uint32_t i = 0; i < expr->product.arg_count; i++) {
                expr_free(expr->product.args[i]);
            }
            free(expr->product.args);
            break;
    }
    
    free(expr);
}

void decl_free(Decl* decl) {
    if (decl == NULL) return;
    
    switch (decl->kind) {
        case DECL_VAR:
            expr_free(decl->var_decl.typ);
            expr_free(decl->var_decl.value);
            break;
        case DECL_FUN:
            for (uint32_t i = 0; i < decl->fun_decl.param_count; i++) {
                expr_free(decl->fun_decl.params[i]->typ);
                free(decl->fun_decl.params[i]);
            }
            free(decl->fun_decl.params);
            expr_free(decl->fun_decl.ret_type);
            expr_free(decl->fun_decl.body);
            break;
        case DECL_INDUCTIVE:
            for (uint32_t i = 0; i < decl->inductive_decl.param_count; i++) {
                expr_free(decl->inductive_decl.params[i]->typ);
                free(decl->inductive_decl.params[i]);
            }
            free(decl->inductive_decl.params);
            expr_free(decl->inductive_decl.arity);
            for (uint32_t i = 0; i < decl->inductive_decl.ctor_count; i++) {
                expr_free(decl->inductive_decl.ctors[i]->typ);
                free(decl->inductive_decl.ctors[i]);
            }
            free(decl->inductive_decl.ctors);
            break;
        case DECL_PRODUCT:
            /* TODO: 释放 product */
            break;
        case DECL_AXIOM:
            expr_free(decl->axiom_decl.typ);
            break;
        case DECL_EXAMPLE:
            expr_free(decl->example_decl.typ);
            expr_free(decl->example_decl.value);
            break;
        case DECL_EQUATION:
            expr_free(decl->var_decl.typ);
            expr_free(decl->var_decl.value);
            break;
    }
    
    free(decl);
}
