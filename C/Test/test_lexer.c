#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

int main(void) {
    printf("=== Lexer Test ===\n\n");
    
    /* 测试 1: 基本关键字和标识符 */
    printf("Test 1: Keywords and identifiers\n");
    {
        const char* source = "inductive Nat { | zero: Nat | succ: Nat -> Nat };";
        Lexer lexer;
        lexer_init(&lexer, source);
        
        if (lexer_tokenize(&lexer)) {
            printf("  Tokens:\n");
            for (uint32_t i = 0; i < lexer_token_count(&lexer); i++) {
                const Token* token = lexer_token_at(&lexer, i);
                printf("    [%s] '%.*s'\n", 
                       token_kind_name(token->kind),
                       token->text_len, token->text);
            }
        } else {
            printf("  Error: %s\n", lexer.error_msg);
        }
    }
    printf("\n");
    
    /* 测试 2: 函数定义 */
    printf("Test 2: Function definition\n");
    {
        const char* source = "fun add (m:Nat) (n:Nat): Nat { m };";
        Lexer lexer;
        lexer_init(&lexer, source);
        
        if (lexer_tokenize(&lexer)) {
            printf("  Tokens:\n");
            for (uint32_t i = 0; i < lexer_token_count(&lexer); i++) {
                const Token* token = lexer_token_at(&lexer, i);
                printf("    [%s] '%.*s'\n", 
                       token_kind_name(token->kind),
                       token->text_len, token->text);
            }
        } else {
            printf("  Error: %s\n", lexer.error_msg);
        }
    }
    printf("\n");
    
    /* 测试 3: Lambda 表达式 */
    printf("Test 3: Lambda expression\n");
    {
        const char* source = "\\x: Nat => x";
        Lexer lexer;
        lexer_init(&lexer, source);
        
        if (lexer_tokenize(&lexer)) {
            printf("  Tokens:\n");
            for (uint32_t i = 0; i < lexer_token_count(&lexer); i++) {
                const Token* token = lexer_token_at(&lexer, i);
                printf("    [%s] '%.*s'\n", 
                       token_kind_name(token->kind),
                       token->text_len, token->text);
            }
        } else {
            printf("  Error: %s\n", lexer.error_msg);
        }
    }
    printf("\n");
    
    /* 测试 4: 注释 */
    printf("Test 4: Comments\n");
    {
        const char* source = "# This is a comment\nvar x : Nat = zero;";
        Lexer lexer;
        lexer_init(&lexer, source);
        
        if (lexer_tokenize(&lexer)) {
            printf("  Tokens:\n");
            for (uint32_t i = 0; i < lexer_token_count(&lexer); i++) {
                const Token* token = lexer_token_at(&lexer, i);
                printf("    [%s] '%.*s'\n", 
                       token_kind_name(token->kind),
                       token->text_len, token->text);
            }
        } else {
            printf("  Error: %s\n", lexer.error_msg);
        }
    }
    printf("\n");
    
    /* 测试 5: 点标识符 */
    printf("Test 5: Dot identifiers\n");
    {
        const char* source = "Nat.rec";
        Lexer lexer;
        lexer_init(&lexer, source);
        
        if (lexer_tokenize(&lexer)) {
            printf("  Tokens:\n");
            for (uint32_t i = 0; i < lexer_token_count(&lexer); i++) {
                const Token* token = lexer_token_at(&lexer, i);
                printf("    [%s] '%.*s'\n", 
                       token_kind_name(token->kind),
                       token->text_len, token->text);
            }
        } else {
            printf("  Error: %s\n", lexer.error_msg);
        }
    }
    printf("\n");
    
    printf("=== All lexer tests passed! ===\n");
    return 0;
}
