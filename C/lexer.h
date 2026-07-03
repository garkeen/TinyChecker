#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Token 类型
 * ============================================================ */

typedef enum {
    /* 关键字 */
    TOKEN_VAR,
    TOKEN_FUN,
    TOKEN_CLAIM,
    TOKEN_THEOREM,
    TOKEN_INDUCTIVE,
    TOKEN_SUM,
    TOKEN_PRODUCT,
    TOKEN_AXIOM,
    TOKEN_EXAMPLE,
    TOKEN_LET,
    TOKEN_IN,
    TOKEN_MATCH,
    TOKEN_CASE,
    TOKEN_AS,
    TOKEN_BIND,
    TOKEN_RETURN,
    TOKEN_WITH,
    TOKEN_OF,
    TOKEN_END,
    
    /* 类型关键字 */
    TOKEN_TYPE,
    TOKEN_PROP,
    
    /* 多字符运算符 */
    TOKEN_DARROW,       /* => */
    TOKEN_ARROW,        /* -> */
    TOKEN_EQEQ,         /* == */
    
    /* 单字符运算符 */
    TOKEN_LBRACE,       /* { */
    TOKEN_RBRACE,       /* } */
    TOKEN_LPAREN,       /* ( */
    TOKEN_RPAREN,       /* ) */
    TOKEN_LBRACKET,     /* [ */
    TOKEN_RBRACKET,     /* ] */
    TOKEN_LANGLE,       /* < */
    TOKEN_RANGLE,       /* > */
    TOKEN_COLON,        /* : */
    TOKEN_EQ,           /* = */
    TOKEN_LAMBDA,       /* \ */
    TOKEN_SEMI,         /* ; */
    TOKEN_COMMA,        /* , */
    TOKEN_BAR,          /* | */
    
    /* 标识符 */
    TOKEN_IDENT,
    TOKEN_DOT,          /* module.name */
    
    /* 特殊 */
    TOKEN_EOF,
    TOKEN_ERROR,
} TokenKind;

/* ============================================================
 * Token 结构
 * ============================================================ */

typedef struct {
    TokenKind kind;
    const char* text;       /* token 文本（指向源代码） */
    uint32_t text_len;      /* 文本长度 */
    uint32_t row;
    uint32_t col;
    uint32_t start_pos;     /* 在源代码中的起始位置 */
    uint32_t end_pos;       /* 在源代码中的结束位置 */
} Token;

/* ============================================================
 * Lexer 结构
 * ============================================================ */

#define MAX_TOKENS 4096

typedef struct {
    const char* source;
    uint32_t source_len;
    uint32_t pos;
    uint32_t row;
    uint32_t col;
    
    Token tokens[MAX_TOKENS];
    uint32_t token_count;
    
    /* 括号匹配计数 */
    int32_t brace_depth;    /* { } */
    int32_t paren_depth;    /* ( ) */
    int32_t bracket_depth;  /* [ ] */
    int32_t angle_depth;    /* < > */
    
    bool has_error;
    char error_msg[256];
} Lexer;

/* ============================================================
 * Lexer 函数
 * ============================================================ */

/* 初始化 lexer */
void lexer_init(Lexer* lexer, const char* source);

/* 进行词法分析 */
bool lexer_tokenize(Lexer* lexer);

/* 获取 token 数量 */
uint32_t lexer_token_count(const Lexer* lexer);

/* 获取指定位置的 token */
const Token* lexer_token_at(const Lexer* lexer, uint32_t index);

/* 获取 token 类型名称（用于调试） */
const char* token_kind_name(TokenKind kind);

#endif /* LEXER_H */
