#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "surface.h"
#include <stdbool.h>

/* ============================================================
 * Parser 结构
 * ============================================================ */

#define MAX_DECLS 1024

typedef struct {
    const Token* tokens;
    uint32_t token_count;
    uint32_t pos;
    
    Decl* decls[MAX_DECLS];
    uint32_t decl_count;
    
    bool has_error;
    char error_msg[256];
} Parser;

/* ============================================================
 * Parser 函数
 * ============================================================ */

/* 初始化 parser */
void parser_init(Parser* parser, const Token* tokens, uint32_t token_count);

/* 解析 tokens 为声明列表 */
bool parser_parse(Parser* parser);

/* 获取声明数量 */
uint32_t parser_decl_count(const Parser* parser);

/* 获取指定位置的声明 */
Decl* parser_decl_at(const Parser* parser, uint32_t index);

#endif /* PARSER_H */
