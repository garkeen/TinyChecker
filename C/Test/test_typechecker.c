#include <stdio.h>
#include <stdlib.h>
#include "term.h"
#include "core_ops.h"
#include "context.h"
#include "runtime.h"
#include "reducer.h"
#include "typechecker.h"

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
    printf("=== TypeChecker Test ===\n\n");
    
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
    
    Term* succ_type = term_new_pi("_", nat, nat);
    global_ctx_register(&gctx, (GlobalEntry){
        .name = "succ",
        .typ = succ_type,
        .value = NULL,
        .kind = GLOBAL_DATA_CTOR
    });
    
    /* 初始化类型检查器 */
    TypeChecker tc;
    typechecker_init(&tc, &gctx, CONV_WHN);
    
    /* 测试 1: 基本类型推导 */
    printf("Test 1: Basic type inference\n");
    
    Context ctx;
    ctx_init(&ctx);
    
    /* Type : Type */
    Term* ty_type = typechecker_infer(&tc, term_new_type(), &ctx);
    printf("  Type : "); term_print(ty_type); printf("\n");
    
    /* Nat : Type */
    Term* ty_nat = typechecker_infer(&tc, nat, &ctx);
    printf("  Nat : "); term_print(ty_nat); printf("\n");
    
    /* zero : Nat */
    Term* ty_zero = typechecker_infer(&tc, term_new_global("zero"), &ctx);
    printf("  zero : "); term_print(ty_zero); printf("\n\n");
    
    /* 测试 2: Lambda 类型推导 */
    printf("Test 2: Lambda type inference\n");
    
    /* \x: Nat. x : (x: Nat) -> Nat */
    Term* lam_id = term_new_lam("x", nat, term_new_var(0, "x"));
    Term* ty_lam_id = typechecker_infer(&tc, lam_id, &ctx);
    printf("  \\x: Nat. x : "); term_print(ty_lam_id); printf("\n");
    
    /* \x: Nat. zero : (x: Nat) -> Nat */
    Term* lam_const = term_new_lam("x", nat, term_new_global("zero"));
    Term* ty_lam_const = typechecker_infer(&tc, lam_const, &ctx);
    printf("  \\x: Nat. zero : "); term_print(ty_lam_const); printf("\n\n");
    
    /* 测试 3: 应用类型推导 */
    printf("Test 3: Application type inference\n");
    
    /* (\x: Nat. x) zero : Nat */
    Term* app = term_new_app(lam_id, term_new_global("zero"));
    Term* ty_app = typechecker_infer(&tc, app, &ctx);
    printf("  (\\x: Nat. x) zero : "); term_print(ty_app); printf("\n\n");
    
    /* 测试 4: 类型检查 */
    printf("Test 4: Type checking\n");
    
    /* 检查 zero : Nat */
    bool chk1 = typechecker_check(&tc, term_new_global("zero"), nat, &ctx);
    printf("  zero : Nat ? %s\n", chk1 ? "true" : "false");
    
    /* 检查 (\x: Nat. x) : (x: Nat) -> Nat */
    Term* expected_type = term_new_pi("_", nat, nat);
    bool chk2 = typechecker_check(&tc, lam_id, expected_type, &ctx);
    printf("  (\\x: Nat. x) : Nat -> Nat ? %s\n", chk2 ? "true" : "false");
    
    /* 检查 (\x: Nat. x) : Nat -> Type（应该失败） */
    Term* wrong_type = term_new_pi("_", nat, term_new_type());
    bool chk3 = typechecker_check(&tc, lam_id, wrong_type, &ctx);
    printf("  (\\x: Nat. x) : Nat -> Type ? %s\n", chk3 ? "true" : "false");
    
    printf("\n=== All typechecker tests passed! ===\n");
    return 0;
}
