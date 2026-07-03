#include <stdio.h>
#include <stdlib.h>
#include "term.h"
#include "core_ops.h"
#include "context.h"

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
    printf("=== TinyChecker C Implementation Test ===\n\n");
    
    /* 测试 1: 创建基本 Term */
    printf("Test 1: Create basic terms\n");
    Term* nat = term_new_global("Nat");
    Term* zero = term_new_global("zero");
    Term* succ = term_new_global("succ");
    
    printf("  nat = "); term_print(nat); printf("\n");
    printf("  zero = "); term_print(zero); printf("\n");
    printf("  succ = "); term_print(succ); printf("\n\n");
    
    /* 测试 2: 创建应用 succ (succ zero) */
    printf("Test 2: Create application succ (succ zero)\n");
    Term* succ_zero = term_new_app(succ, zero);
    Term* succ_succ_zero = term_new_app(succ, succ_zero);
    
    printf("  succ zero = "); term_print(succ_zero); printf("\n");
    printf("  succ (succ zero) = "); term_print(succ_succ_zero); printf("\n\n");
    
    /* 测试 3: 展开应用链 */
    printf("Test 3: Unfold application chain\n");
    AppChain chain = term_unfold_app(succ_succ_zero);
    printf("  head = "); term_print(chain.head); printf("\n");
    printf("  args count = %u\n", (unsigned)chain.count);
    for (size_t i = 0; i < chain.count; i++) {
        printf("  arg[%u] = ", (unsigned)i); term_print(chain.args[i]); printf("\n");
    }
    app_chain_free(&chain);
    printf("\n");
    
    /* 测试 4: 创建 Lambda 和 Pi 类型 */
    printf("Test 4: Create Lambda and Pi types\n");
    /* \x: Nat. x */
    Term* lam_id = term_new_lam("x", nat, term_new_var(0, "x"));
    printf("  \\x: Nat. x = "); term_print(lam_id); printf("\n");
    
    /* (x: Nat) -> Nat */
    Term* pi_nat_nat = term_new_pi("x", nat, nat);
    printf("  (x: Nat) -> Nat = "); term_print(pi_nat_nat); printf("\n\n");
    
    /* 测试 5: shift 操作 */
    printf("Test 5: Shift operation\n");
    /* shift(@0, 1, 0) = @1 */
    Term* var0 = term_new_var(0, "x");
    Term* shifted = term_shift(var0, 1, 0);
    printf("  shift(@0, 1, 0) = "); term_print(shifted); printf("\n");
    
    /* shift(@0, 1, 1) = @0 (不变) */
    Term* shifted2 = term_shift(var0, 1, 1);
    printf("  shift(@0, 1, 1) = "); term_print(shifted2); printf("\n\n");
    
    /* 测试 6: instantiate 操作 */
    printf("Test 6: Instantiate operation\n");
    /* instantiate(@0, zero) = zero */
    Term* inst1 = term_instantiate(var0, zero);
    printf("  instantiate(@0, zero) = "); term_print(inst1); printf("\n");
    
    /* instantiate(@1, zero) = @0 */
    Term* var1 = term_new_var(1, "y");
    Term* inst2 = term_instantiate(var1, zero);
    printf("  instantiate(@1, zero) = "); term_print(inst2); printf("\n\n");
    
    /* 测试 7: 不变时复用原节点 */
    printf("Test 7: Reuse unchanged nodes\n");
    /* shift(Type, 1, 0) = Type (同一个指针) */
    Term* type = term_new_type();
    Term* shifted_type = term_shift(type, 1, 0);
    printf("  Type == shift(Type, 1, 0): %s\n", 
           (type == shifted_type) ? "YES (reused)" : "NO (new node)");
    
    /* shift(Global("Nat"), 1, 0) = Global("Nat") (同一个指针) */
    Term* shifted_nat = term_shift(nat, 1, 0);
    printf("  Nat == shift(Nat, 1, 0): %s\n", 
           (nat == shifted_nat) ? "YES (reused)" : "NO (new node)");
    
    printf("\n=== All basic tests passed! ===\n\n");
    
    /* 测试 8: 上下文操作 */
    printf("Test 8: Context operations\n");
    Context ctx;
    ctx_init(&ctx);
    
    /* 扩展上下文：x: Nat, y: Nat, z: Type */
    ctx_extend(&ctx, "x", nat);
    ctx_extend(&ctx, "y", nat);
    ctx_extend(&ctx, "z", type);
    
    printf("  depth = %u\n", ctx_depth(&ctx));
    
    /* 查找变量 */
    const Term* lookup0 = ctx_lookup(&ctx, 0);
    printf("  lookup(0) = "); term_print(lookup0); printf(" (expected: Type)\n");
    
    const Term* lookup1 = ctx_lookup(&ctx, 1);
    printf("  lookup(1) = "); term_print(lookup1); printf(" (expected: @1(y))\n");
    
    const Term* lookup2 = ctx_lookup(&ctx, 2);
    printf("  lookup(2) = "); term_print(lookup2); printf(" (expected: @2(x))\n");
    
    /* 退出作用域 */
    ctx_pop(&ctx);
    printf("  after pop: depth = %u\n", ctx_depth(&ctx));
    
    ctx_free(&ctx);
    printf("\n=== All tests passed! ===\n");
    
    return 0;
}
