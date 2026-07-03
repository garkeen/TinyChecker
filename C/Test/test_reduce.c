#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pipeline.h"
#include "core_ops.h"

int main(void) {
    PipelineResult result;
    if (!check_file("../Example/Nat.pind", CONV_WHN, &result)) {
        printf("Pipeline failed: %s\n", result.error_msg);
        return 1;
    }
    
    GlobalContext* ctx = &result.global_ctx;
    Reducer r;
    reducer_init(&r, ctx, CONV_WHN);
    
    /* 构建 mul m (succ n) */
    Term* m = term_new_var(1, "m");
    Term* n = term_new_var(0, "n");
    Term* succ_n = term_new_app(term_new_global("succ"), n);
    Term* mul = term_new_global("mul");
    Term* mul_m_succ_n = term_new_app(term_new_app(mul, m), succ_n);
    
    printf("mul m (succ n) = "); print_term(mul_m_succ_n); printf("\n");
    
    /* WHNF */
    Term* whnf1 = reducer_whnf(&r, mul_m_succ_n);
    printf("WHNF(mul m (succ n)) = "); print_term(whnf1); printf("\n");
    
    /* 构建 add m (mul m n) */
    Term* mul_m_n = term_new_app(term_new_app(mul, m), n);
    Term* add = term_new_global("add");
    Term* add_m_mul_m_n = term_new_app(term_new_app(add, m), mul_m_n);
    
    printf("add m (mul m n) = "); print_term(add_m_mul_m_n); printf("\n");
    
    Term* whnf2 = reducer_whnf(&r, add_m_mul_m_n);
    printf("WHNF(add m (mul m n)) = "); print_term(whnf2); printf("\n");
    
    /* is_def_eq */
    bool eq = reducer_is_def_eq(&r, mul_m_succ_n, add_m_mul_m_n);
    printf("is_def_eq? %s\n", eq ? "YES" : "NO");
    
    /* 额外：WHNF(mul m n) */
    Term* whnf3 = reducer_whnf(&r, mul_m_n);
    printf("WHNF(mul m n) = "); print_term(whnf3); printf("\n");
    
    pipeline_result_free(&result);
    return 0;
}
