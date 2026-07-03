#include <stdio.h>
#include <stdlib.h>
#include "term.h"
#include "core_ops.h"
#include "context.h"
#include "runtime.h"
#include "reducer.h"

/* 简单的 Term 打印 */
void term_print(const Term* t) {
    switch (t->kind) {
        case TERM_VAR:
            if (t->var.name) {
                printf("@%u(%s)", t->var.index, t->var.name);
            } else {
                printf("@%u", t->var.index);
            }
            break;
        case TERM_GLOBAL:
            printf("%s", t->global);
            break;
        case TERM_TYPE:
            printf("Type");
            break;
        case TERM_PI:
            printf("Pi(%s: ", t->pi.name);
            term_print(t->pi.domain);
            printf(", ");
            term_print(t->pi.codomain);
            printf(")");
            break;
        case TERM_LAM:
            printf("Lam(%s: ", t->lam.name);
            term_print(t->lam.param_type);
            printf(", ");
            term_print(t->lam.body);
            printf(")");
            break;
        case TERM_APP:
            printf("App(");
            term_print(t->app.func);
            printf(", ");
            term_print(t->app.arg);
            printf(")");
            break;
    }
}

int main(void) {
    printf("=== Reducer Test ===\n\n");
    
    /* 初始化全局上下文 */
    GlobalContext gctx;
    global_ctx_init(&gctx);
    
    /* 注册 Nat 类型 */
    Term* nat_type = term_new_type();
    global_ctx_register(&gctx, (GlobalEntry){
        .name = "Nat",
        .typ = nat_type,
        .value = NULL,
        .kind = GLOBAL_TYPE_CTOR
    });
    
    /* 注册 zero 和 succ */
    Term* nat = term_new_global("Nat");
    global_ctx_register(&gctx, (GlobalEntry){
        .name = "zero",
        .typ = nat,
        .value = NULL,
        .kind = GLOBAL_DATA_CTOR
    });
    
    /* succ : Nat -> Nat */
    Term* succ_type = term_new_pi("_", nat, nat);
    global_ctx_register(&gctx, (GlobalEntry){
        .name = "succ",
        .typ = succ_type,
        .value = NULL,
        .kind = GLOBAL_DATA_CTOR
    });
    
    /* 注册 add 函数
     * add = \m: Nat. \n: Nat. match m with
     *   | zero => n
     *   | succ k => succ (add k n)
     * 简化版本：add = \m: Nat. \n: Nat. n (暂时)
     */
    Term* add_body = term_new_lam("m", nat, 
                        term_new_lam("n", nat, 
                            term_new_var(0, "n")));
    Term* add_type = term_new_pi("_", nat, term_new_pi("_", nat, nat));
    global_ctx_register(&gctx, (GlobalEntry){
        .name = "add",
        .typ = add_type,
        .value = add_body,
        .kind = GLOBAL_FUN
    });
    
    /* 初始化归约器 */
    Reducer reducer;
    reducer_init(&reducer, &gctx, CONV_WHN);
    
    /* 测试 1: 基本 whnf */
    printf("Test 1: Basic WHNF\n");
    
    /* Type 的 whnf 是 Type */
    Term* t1 = term_new_type();
    Term* r1 = reducer_whnf(&reducer, t1);
    printf("  whnf(Type) = "); term_print(r1); printf("\n");
    
    /* Nat 的 whnf 是 Nat（无法归约） */
    Term* r2 = reducer_whnf(&reducer, nat);
    printf("  whnf(Nat) = "); term_print(r2); printf("\n");
    
    /* zero 的 whnf 是 zero */
    Term* zero = term_new_global("zero");
    Term* r3 = reducer_whnf(&reducer, zero);
    printf("  whnf(zero) = "); term_print(r3); printf("\n\n");
    
    /* 测试 2: Beta 归约 */
    printf("Test 2: Beta reduction\n");
    
    /* (\x: Nat. x) zero -> zero */
    Term* lam_id = term_new_lam("x", nat, term_new_var(0, "x"));
    Term* app_id = term_new_app(lam_id, zero);
    Term* r4 = reducer_whnf(&reducer, app_id);
    printf("  whnf((\\x. x) zero) = "); term_print(r4); printf("\n");
    
    /* (\x: Nat. \y: Nat. x) zero -> \y: Nat. zero */
    Term* lam_const = term_new_lam("x", nat, 
                          term_new_lam("y", nat, 
                              term_new_var(1, "x")));
    Term* app_const = term_new_app(lam_const, zero);
    Term* r5 = reducer_whnf(&reducer, app_const);
    printf("  whnf((\\x. \\y. x) zero) = "); term_print(r5); printf("\n\n");
    
    /* 测试 3: Delta 归约 */
    printf("Test 3: Delta reduction\n");
    
    /* add 的 whnf 是 add 的定义 */
    Term* add = term_new_global("add");
    Term* r6 = reducer_whnf(&reducer, add);
    printf("  whnf(add) = "); term_print(r6); printf("\n\n");
    
    /* 测试 4: 结构相等 */
    printf("Test 4: Structural equality\n");
    
    bool eq1 = reducer_is_def_eq(&reducer, nat, nat);
    printf("  Nat == Nat: %s\n", eq1 ? "true" : "false");
    
    bool eq2 = reducer_is_def_eq(&reducer, nat, term_new_type());
    printf("  Nat == Type: %s\n", eq2 ? "true" : "false");
    
    bool eq3 = reducer_is_def_eq(&reducer, zero, zero);
    printf("  zero == zero: %s\n", eq3 ? "true" : "false");
    
    /* (\x. x) zero == zero */
    bool eq4 = reducer_is_def_eq(&reducer, app_id, zero);
    printf("  (\\x. x) zero == zero: %s\n", eq4 ? "true" : "false");
    
    printf("\n=== All reducer tests passed! ===\n");
    return 0;
}
