#ifndef TERM_H
#define TERM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * Term - 核心数据结构（不可变）
 * ============================================================ */

typedef enum {
    TERM_VAR,       /* de Bruijn index */
    TERM_GLOBAL,    /* 全局名（字符串） */
    TERM_TYPE,      /* Type */
    TERM_PI,        /* (x: A) -> B */
    TERM_LAM,       /* \x: A. t */
    TERM_APP,       /* f a */
} TermKind;

typedef struct Term Term;

struct Term {
    TermKind kind;
    union {
        struct {
            uint32_t index;
            const char* name;   /* 可选，用于调试 */
        } var;
        const char* global;     /* 全局名 */
        /* TYPE: 无数据 */
        struct {
            const char* name;
            const Term* domain;
            const Term* codomain;
        } pi;
        struct {
            const char* name;
            const Term* param_type;
            const Term* body;
        } lam;
        struct {
            const Term* func;
            const Term* arg;
        } app;
    };
};

/* ============================================================
 * Term 构造函数
 * ============================================================ */

Term* term_new_var(uint32_t index, const char* name);
Term* term_new_global(const char* name);
Term* term_new_type(void);
Term* term_new_pi(const char* name, const Term* domain, const Term* codomain);
Term* term_new_lam(const char* name, const Term* param_type, const Term* body);
Term* term_new_app(const Term* func, const Term* arg);

/* ============================================================
 * Term 辅助函数
 * ============================================================ */

/* 展开应用链：App(App(f, a1), a2) -> (f, [a1, a2]) */
typedef struct {
    const Term* head;
    const Term** args;
    size_t count;
} AppChain;

AppChain term_unfold_app(const Term* term);

/* 从 head 和 args 构建应用链 */
Term* term_mk_apps(const Term* head, const Term** args, size_t count);

/* 释放 AppChain 的 args 数组（不释放 Term 本身） */
void app_chain_free(AppChain* chain);

#endif /* TERM_H */
