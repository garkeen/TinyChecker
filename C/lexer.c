#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ============================================================
 * 关键字表
 * ============================================================ */

typedef struct {
    const char* text;
    TokenKind kind;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"var", TOKEN_VAR},
    {"fun", TOKEN_FUN},
    {"claim", TOKEN_CLAIM},
    {"theorem", TOKEN_THEOREM},
    {"inductive", TOKEN_INDUCTIVE},
    {"sum", TOKEN_SUM},
    {"product", TOKEN_PRODUCT},
    {"axiom", TOKEN_AXIOM},
    {"example", TOKEN_EXAMPLE},
    {"let", TOKEN_LET},
    {"in", TOKEN_IN},
    {"match", TOKEN_MATCH},
    {"case", TOKEN_CASE},
    {"as", TOKEN_AS},
    {"bind", TOKEN_BIND},
    {"return", TOKEN_RETURN},
    {"with", TOKEN_WITH},
    {"of", TOKEN_OF},
    {"end", TOKEN_END},
    {NULL, TOKEN_EOF}  /* 哨兵 */
};

static const KeywordEntry type_keywords[] = {
    {"Type", TOKEN_TYPE},
    {"Prop", TOKEN_PROP},
    {NULL, TOKEN_EOF}
};

/* ============================================================
 * 辅助函数
 * ============================================================ */

static char current_char(Lexer* lexer) {
    if (lexer->pos >= lexer->source_len) return '\0';
    return lexer->source[lexer->pos];
}

static char peek_char(Lexer* lexer, uint32_t offset) {
    uint32_t index = lexer->pos + offset;
    if (index >= lexer->source_len) return '\0';
    return lexer->source[index];
}

static void advance_char(Lexer* lexer, char ch) {
    lexer->pos++;
    if (ch == '\n') {
        lexer->row++;
        lexer->col = 1;
    } else if (ch == '\t') {
        lexer->col += 2;  /* TAB_WIDTH = 2 */
    } else {
        lexer->col++;
    }
}

static bool add_token(Lexer* lexer, TokenKind kind, uint32_t start_pos, uint32_t end_pos) {
    if (lexer->token_count >= MAX_TOKENS) {
        snprintf(lexer->error_msg, sizeof(lexer->error_msg), "too many tokens");
        lexer->has_error = true;
        return false;
    }
    
    Token* token = &lexer->tokens[lexer->token_count++];
    token->kind = kind;
    token->text = &lexer->source[start_pos];
    token->text_len = end_pos - start_pos;
    token->row = lexer->row;
    token->col = lexer->col;
    token->start_pos = start_pos;
    token->end_pos = end_pos;
    return true;
}

static void set_error(Lexer* lexer, const char* msg) {
    snprintf(lexer->error_msg, sizeof(lexer->error_msg), "%s at %u:%u", msg, lexer->row, lexer->col);
    lexer->has_error = true;
}

/* ============================================================
 * 词法分析核心
 * ============================================================ */

static void skip_whitespace(Lexer* lexer) {
    while (lexer->pos < lexer->source_len) {
        char ch = current_char(lexer);
        if (ch == ' ' || ch == '\r' || ch == '\t' || ch == '\n') {
            advance_char(lexer, ch);
        } else if (ch == '#') {
            /* 注释：跳到行尾 */
            while (lexer->pos < lexer->source_len && current_char(lexer) != '\n') {
                advance_char(lexer, current_char(lexer));
            }
        } else {
            break;
        }
    }
}

static bool lex_ident_or_keyword(Lexer* lexer) {
    uint32_t start_pos = lexer->pos;
    uint32_t start_col = lexer->col;
    
    /* 读取标识符 */
    while (lexer->pos < lexer->source_len) {
        char ch = current_char(lexer);
        if (isalnum(ch) || ch == '_') {
            advance_char(lexer, ch);
        } else {
            break;
        }
    }
    
    uint32_t len = lexer->pos - start_pos;
    const char* text = &lexer->source[start_pos];
    
    /* 检查是否是点标识符 (Module.name) */
    if (lexer->pos < lexer->source_len && current_char(lexer) == '.') {
        uint32_t dot_pos = lexer->pos;
        advance_char(lexer, '.');
        
        if (lexer->pos < lexer->source_len && (isalpha(current_char(lexer)) || current_char(lexer) == '_')) {
            while (lexer->pos < lexer->source_len) {
                char ch = current_char(lexer);
                if (isalnum(ch) || ch == '_') {
                    advance_char(lexer, ch);
                } else {
                    break;
                }
            }
            
            /* 检查是否有多个点 */
            if (lexer->pos < lexer->source_len && current_char(lexer) == '.') {
                set_error(lexer, "only one dot is allowed in DOT identifiers");
                return false;
            }
            
            return add_token(lexer, TOKEN_DOT, start_pos, lexer->pos);
        }
        
        /* 不是点标识符，回退 */
        lexer->pos = dot_pos;
        lexer->col = start_col + len;
    }
    
    /* 检查类型关键字 */
    for (const KeywordEntry* kw = type_keywords; kw->text != NULL; kw++) {
        if (strlen(kw->text) == len && memcmp(text, kw->text, len) == 0) {
            return add_token(lexer, kw->kind, start_pos, lexer->pos);
        }
    }
    
    /* 检查普通关键字 */
    for (const KeywordEntry* kw = keywords; kw->text != NULL; kw++) {
        if (strlen(kw->text) == len && memcmp(text, kw->text, len) == 0) {
            return add_token(lexer, kw->kind, start_pos, lexer->pos);
        }
    }
    
    /* 是标识符 */
    return add_token(lexer, TOKEN_IDENT, start_pos, lexer->pos);
}

static bool lex_single_char(Lexer* lexer) {
    char ch = current_char(lexer);
    uint32_t start_pos = lexer->pos;
    TokenKind kind;
    
    switch (ch) {
        case '{': kind = TOKEN_LBRACE; lexer->brace_depth++; break;
        case '}': kind = TOKEN_RBRACE; lexer->brace_depth--; break;
        case '(': kind = TOKEN_LPAREN; lexer->paren_depth++; break;
        case ')': kind = TOKEN_RPAREN; lexer->paren_depth--; break;
        case '[': kind = TOKEN_LBRACKET; lexer->bracket_depth++; break;
        case ']': kind = TOKEN_RBRACKET; lexer->bracket_depth--; break;
        case '<': kind = TOKEN_LANGLE; lexer->angle_depth++; break;
        case '>': kind = TOKEN_RANGLE; lexer->angle_depth--; break;
        case ':': kind = TOKEN_COLON; break;
        case '=': kind = TOKEN_EQ; break;
        case '\\': kind = TOKEN_LAMBDA; break;
        case ';': kind = TOKEN_SEMI; break;
        case ',': kind = TOKEN_COMMA; break;
        case '|': kind = TOKEN_BAR; break;
        default: return false;
    }
    
    advance_char(lexer, ch);
    return add_token(lexer, kind, start_pos, lexer->pos);
}

/* ============================================================
 * 公共接口
 * ============================================================ */

void lexer_init(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->source_len = strlen(source);
    lexer->pos = 0;
    lexer->row = 1;
    lexer->col = 1;
    lexer->token_count = 0;
    lexer->brace_depth = 0;
    lexer->paren_depth = 0;
    lexer->bracket_depth = 0;
    lexer->angle_depth = 0;
    lexer->has_error = false;
    lexer->error_msg[0] = '\0';
}

bool lexer_tokenize(Lexer* lexer) {
    while (lexer->pos < lexer->source_len) {
        skip_whitespace(lexer);
        if (lexer->pos >= lexer->source_len) break;
        
        char ch = current_char(lexer);
        char next_ch = peek_char(lexer, 1);
        
        /* 多字符运算符 */
        if (ch == '=' && next_ch == '>') {
            advance_char(lexer, '=');
            advance_char(lexer, '>');
            if (!add_token(lexer, TOKEN_DARROW, lexer->pos - 2, lexer->pos)) return false;
            continue;
        }
        if (ch == '-' && next_ch == '>') {
            advance_char(lexer, '-');
            advance_char(lexer, '>');
            if (!add_token(lexer, TOKEN_ARROW, lexer->pos - 2, lexer->pos)) return false;
            continue;
        }
        if (ch == '=' && next_ch == '=') {
            advance_char(lexer, '=');
            advance_char(lexer, '=');
            if (!add_token(lexer, TOKEN_EQEQ, lexer->pos - 2, lexer->pos)) return false;
            continue;
        }
        
        /* 单字符运算符 */
        if (ch == '{' || ch == '}' || ch == '(' || ch == ')' ||
            ch == '[' || ch == ']' || ch == '<' || ch == '>' ||
            ch == ':' || ch == '=' || ch == '\\' || ch == ';' ||
            ch == ',' || ch == '|') {
            if (!lex_single_char(lexer)) return false;
            continue;
        }
        
        /* 标识符或关键字 */
        if (isalpha(ch) || ch == '_') {
            if (!lex_ident_or_keyword(lexer)) return false;
            continue;
        }
        
        /* 未知字符 */
        char msg[64];
        snprintf(msg, sizeof(msg), "unexpected character '%c'", ch);
        set_error(lexer, msg);
        return false;
    }
    
    /* 检查括号匹配 */
    if (lexer->brace_depth != 0 || lexer->paren_depth != 0 || 
        lexer->bracket_depth != 0 || lexer->angle_depth != 0) {
        set_error(lexer, "unbalanced delimiters");
        return false;
    }
    
    /* 添加 EOF token */
    add_token(lexer, TOKEN_EOF, lexer->pos, lexer->pos);
    
    return !lexer->has_error;
}

uint32_t lexer_token_count(const Lexer* lexer) {
    return lexer->token_count;
}

const Token* lexer_token_at(const Lexer* lexer, uint32_t index) {
    if (index >= lexer->token_count) return NULL;
    return &lexer->tokens[index];
}

const char* token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOKEN_VAR: return "VAR";
        case TOKEN_FUN: return "FUN";
        case TOKEN_CLAIM: return "CLAIM";
        case TOKEN_THEOREM: return "THEOREM";
        case TOKEN_INDUCTIVE: return "INDUCTIVE";
        case TOKEN_SUM: return "SUM";
        case TOKEN_PRODUCT: return "PRODUCT";
        case TOKEN_AXIOM: return "AXIOM";
        case TOKEN_EXAMPLE: return "EXAMPLE";
        case TOKEN_LET: return "LET";
        case TOKEN_IN: return "IN";
        case TOKEN_MATCH: return "MATCH";
        case TOKEN_CASE: return "CASE";
        case TOKEN_AS: return "AS";
        case TOKEN_BIND: return "BIND";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_WITH: return "WITH";
        case TOKEN_OF: return "OF";
        case TOKEN_END: return "END";
        case TOKEN_TYPE: return "TYPE";
        case TOKEN_PROP: return "PROP";
        case TOKEN_DARROW: return "DARROW";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_EQEQ: return "EQEQ";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_LANGLE: return "LANGLE";
        case TOKEN_RANGLE: return "RANGLE";
        case TOKEN_COLON: return "COLON";
        case TOKEN_EQ: return "EQ";
        case TOKEN_LAMBDA: return "LAMBDA";
        case TOKEN_SEMI: return "SEMI";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_BAR: return "BAR";
        case TOKEN_IDENT: return "IDENT";
        case TOKEN_DOT: return "DOT";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
    }
    return "UNKNOWN";
}
