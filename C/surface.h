#ifndef SURFACE_H
#define SURFACE_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 表面语法 AST 节点
 * ============================================================ */

typedef enum {
    EXPR_ATOM,
    EXPR_ARROW,
    EXPR_PI,
    EXPR_LAMBDA,
    EXPR_APP,
    EXPR_LET,
    EXPR_EQ,
    EXPR_MATCH,
    EXPR_CASE,
    EXPR_PRODUCT,
} ExprKind;

typedef enum {
    DECL_VAR,
    DECL_FUN,
    DECL_INDUCTIVE,
    DECL_PRODUCT,
    DECL_AXIOM,
    DECL_EXAMPLE,
    DECL_EQUATION,
} DeclKind;

/* 前向声明 */
typedef struct Expr Expr;
typedef struct Decl Decl;
typedef struct Param Param;
typedef struct MatchBranch MatchBranch;
typedef struct CaseBranch CaseBranch;
typedef struct CtorDecl CtorDecl;
typedef struct FieldDecl FieldDecl;

/* ============================================================
 * 表达式节点
 * ============================================================ */

struct Expr {
    ExprKind kind;
    uint32_t surface_id;    /* 用于调试 */
    union {
        struct {
            const char* text;
        } atom;
        struct {
            Expr* domain;
            Expr* codomain;
        } arrow;
        struct {
            const char* name;
            Expr* domain;
            Expr* codomain;
        } pi;
        struct {
            Param* param;
            Expr* body;
        } lambda;
        struct {
            Expr* func;
            Expr* arg;
        } app;
        struct {
            const char* name;
            Expr* typ;
            Expr* value;
            Expr* body;
        } let;
        struct {
            Expr* typ;
            Expr* lhs;
            Expr* rhs;
        } eq;
        struct {
            Expr* scrutinee;
            char* alias;
            char* inductive;
            Expr** family_args;
            uint32_t family_arg_count;
            char** bind_names;
            uint32_t bind_name_count;
            Expr* motive_body;
            MatchBranch** branches;
            uint32_t branch_count;
        } match;
        struct {
            Expr* scrutinee;
            char* alias;
            char* sum_type;
            Expr** type_args;
            uint32_t type_arg_count;
            char** bind_names;
            uint32_t bind_name_count;
            Expr* motive_body;
            CaseBranch** branches;
            uint32_t branch_count;
        } case_expr;
        struct {
            const char* type_name;
            Expr** args;
            uint32_t arg_count;
        } product;
    };
};

/* ============================================================
 * 参数和分支
 * ============================================================ */

struct Param {
    const char* name;       /* 可能为 NULL（匿名参数） */
    Expr* typ;
};

struct MatchBranch {
    char* ctor;
    char** fields;
    uint32_t field_count;
    char** ihs;
    uint32_t ih_count;
    Expr* body;
};

struct CaseBranch {
    char* ctor;
    char** fields;
    uint32_t field_count;
    Expr* body;
};

struct CtorDecl {
    const char* name;
    Expr* typ;
};

struct FieldDecl {
    const char* name;
    Expr* typ;
};

/* ============================================================
 * 声明节点
 * ============================================================ */

struct Decl {
    DeclKind kind;
    uint32_t surface_id;
    union {
        struct {
            const char* kind_str;   /* "var" 或 "claim" */
            const char* name;
            Expr* typ;
            Expr* value;
        } var_decl;
        struct {
            const char* kind_str;   /* "fun" 或 "theorem" */
            const char* name;
            Param** params;
            uint32_t param_count;
            Expr* ret_type;
            Expr* body;
        } fun_decl;
        struct {
            const char* kind_str;   /* "inductive" 或 "sum" */
            const char* name;
            Param** params;
            uint32_t param_count;
            Expr* arity;
            CtorDecl** ctors;
            uint32_t ctor_count;
        } inductive_decl;
        struct {
            const char* name;
            FieldDecl** fields;
            uint32_t field_count;
        } product_decl;
        struct {
            const char* name;
            Expr* typ;
        } axiom_decl;
        struct {
            Expr* typ;
            Expr* value;
        } example_decl;
    };
};

/* ============================================================
 * AST 构造函数
 * ============================================================ */

Expr* expr_new_atom(const char* text);
Expr* expr_new_arrow(Expr* domain, Expr* codomain);
Expr* expr_new_pi(const char* name, Expr* domain, Expr* codomain);
Expr* expr_new_lambda(Param* param, Expr* body);
Expr* expr_new_app(Expr* func, Expr* arg);
Expr* expr_new_let(const char* name, Expr* typ, Expr* value, Expr* body);
Expr* expr_new_eq(Expr* typ, Expr* lhs, Expr* rhs);

Decl* decl_new_var(const char* kind, const char* name, Expr* typ, Expr* value);
Decl* decl_new_fun(const char* kind, const char* name, Param** params, uint32_t param_count, Expr* ret_type, Expr* body);
Decl* decl_new_inductive(const char* kind, const char* name, Param** params, uint32_t param_count, Expr* arity, CtorDecl** ctors, uint32_t ctor_count);
Decl* decl_new_axiom(const char* name, Expr* typ);

Param* param_new(const char* name, Expr* typ);
CtorDecl* ctor_decl_new(const char* name, Expr* typ);

/* 释放 AST（深度释放） */
void expr_free(Expr* expr);
void decl_free(Decl* decl);

#endif /* SURFACE_H */
