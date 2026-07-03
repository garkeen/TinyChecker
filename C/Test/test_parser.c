#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "surface.h"

void print_expr(Expr* expr) {
    if (expr == NULL) {
        printf("NULL");
        return;
    }
    switch (expr->kind) {
        case EXPR_ATOM:
            printf("%s", expr->atom.text);
            break;
        case EXPR_ARROW:
            print_expr(expr->arrow.domain);
            printf(" -> ");
            print_expr(expr->arrow.codomain);
            break;
        case EXPR_PI:
            printf("(%s: ", expr->pi.name ? expr->pi.name : "_");
            print_expr(expr->pi.domain);
            printf(") -> ");
            print_expr(expr->pi.codomain);
            break;
        case EXPR_LAMBDA:
            printf("\\%s: ", expr->lambda.param->name ? expr->lambda.param->name : "_");
            print_expr(expr->lambda.param->typ);
            printf(" => ");
            print_expr(expr->lambda.body);
            break;
        case EXPR_APP:
            print_expr(expr->app.func);
            printf(" ");
            print_expr(expr->app.arg);
            break;
        case EXPR_LET:
            printf("let %s: ", expr->let.name ? expr->let.name : "_");
            print_expr(expr->let.typ);
            printf(" = ");
            print_expr(expr->let.value);
            printf(" in ");
            print_expr(expr->let.body);
            break;
        case EXPR_EQ:
            printf("[");
            print_expr(expr->eq.typ);
            printf("] ");
            print_expr(expr->eq.lhs);
            printf(" == ");
            print_expr(expr->eq.rhs);
            break;
        case EXPR_MATCH:
            printf("match ... end");
            break;
        case EXPR_CASE:
            printf("case ... end");
            break;
        case EXPR_PRODUCT:
            printf("%s<...>", expr->product.type_name);
            break;
    }
}

void print_decl(Decl* decl) {
    if (decl == NULL) {
        printf("NULL\n");
        return;
    }
    switch (decl->kind) {
        case DECL_VAR:
            printf("%s %s: ", decl->var_decl.kind_str, decl->var_decl.name);
            print_expr(decl->var_decl.typ);
            printf(" = ");
            print_expr(decl->var_decl.value);
            printf(";\n");
            break;
        case DECL_FUN:
            printf("%s %s", decl->fun_decl.kind_str, decl->fun_decl.name);
            for (uint32_t i = 0; i < decl->fun_decl.param_count; i++) {
                printf(" (%s: ", decl->fun_decl.params[i]->name);
                print_expr(decl->fun_decl.params[i]->typ);
                printf(")");
            }
            printf(" : ");
            print_expr(decl->fun_decl.ret_type);
            printf(" { ");
            print_expr(decl->fun_decl.body);
            printf(" };\n");
            break;
        case DECL_INDUCTIVE:
            printf("%s %s", decl->inductive_decl.kind_str, decl->inductive_decl.name);
            for (uint32_t i = 0; i < decl->inductive_decl.param_count; i++) {
                printf(" (%s: ", decl->inductive_decl.params[i]->name);
                print_expr(decl->inductive_decl.params[i]->typ);
                printf(")");
            }
            if (decl->inductive_decl.arity) {
                printf(" : ");
                print_expr(decl->inductive_decl.arity);
            }
            printf(" {\n");
            for (uint32_t i = 0; i < decl->inductive_decl.ctor_count; i++) {
                printf("  | %s: ", decl->inductive_decl.ctors[i]->name);
                print_expr(decl->inductive_decl.ctors[i]->typ);
                printf("\n");
            }
            printf("};\n");
            break;
        case DECL_AXIOM:
            printf("axiom %s: ", decl->axiom_decl.name);
            print_expr(decl->axiom_decl.typ);
            printf(";\n");
            break;
        case DECL_EXAMPLE:
            printf("example : ");
            print_expr(decl->example_decl.typ);
            printf(" = ");
            print_expr(decl->example_decl.value);
            printf(";\n");
            break;
        case DECL_PRODUCT:
            printf("product %s {\n", decl->product_decl.name);
            for (uint32_t i = 0; i < decl->product_decl.field_count; i++) {
                printf("  %s: ", decl->product_decl.fields[i]->name);
                print_expr(decl->product_decl.fields[i]->typ);
                printf("\n");
            }
            printf("};\n");
            break;
        case DECL_EQUATION:
            printf("equation %s;\n", decl->var_decl.name);
            break;
    }
}

int run_test(const char* name, const char* source) {
    printf("%s\n", name);
    fflush(stdout);
    Lexer lexer;
    lexer_init(&lexer, source);
    printf("  Lexer init OK\n");
    fflush(stdout);
    if (!lexer_tokenize(&lexer)) {
        printf("  Lexer error: %s\n", lexer.error_msg);
        return 0;
    }
    printf("  Tokenize OK\n");
    fflush(stdout);
    Parser parser;
    parser_init(&parser, lexer.tokens, lexer.token_count);
    printf("  Parser init OK\n");
    fflush(stdout);
    if (!parser_parse(&parser)) {
        printf("  Parser error: %s\n", parser.error_msg);
        return 0;
    }
    printf("  Parse OK\n");
    fflush(stdout);
    for (uint32_t i = 0; i < parser_decl_count(&parser); i++) {
        printf("  ");
        print_decl(parser_decl_at(&parser, i));
    }
    return 1;
}

int main(void) {
    printf("=== Parser Test ===\n");
    fflush(stdout);

    int pass = 0, fail = 0;

    printf("Running test 1...\n");
    fflush(stdout);

    if (run_test("Test 1: inductive Nat",
                 "inductive Nat { | zero: Nat | succ: Nat -> Nat };"))
        pass++; else fail++;

    if (run_test("Test 2: fun const",
                 "fun const (x:Nat): Nat -> Nat { \\y: Nat => x };"))
        pass++; else fail++;

    if (run_test("Test 3: let expression",
                 "var x : Nat = let y: Nat = zero in y;"))
        pass++; else fail++;

    if (run_test("Test 4: axiom",
                 "axiom eqRefl : (A:Type) -> (a:A) -> Eq A a a;"))
        pass++; else fail++;

    if (run_test("Test 5: match expression",
                 "fun isZero (n:Nat): Nat { match n as q in Nat return Nat with | zero => zero | succ k => zero end };"))
        pass++; else fail++;

    if (run_test("Test 6: product declaration",
                 "product Pair { fst: Nat, snd: Nat };"))
        pass++; else fail++;

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
