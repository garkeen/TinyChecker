#ifndef PIPELINE_H
#define PIPELINE_H

#include "lexer.h"
#include "parser.h"
#include "elaborator.h"
#include "reducer.h"
#include "typechecker.h"
#include <stdbool.h>

/* ============================================================
 * Pipeline 结果
 * ============================================================ */

typedef struct {
    bool success;
    Lexer lexer;
    Parser parser;
    Elaborator elaborator;
    GlobalContext global_ctx;
    char error_msg[256];
} PipelineResult;

/* ============================================================
 * Pipeline 函数
 * ============================================================ */

/* 运行完整管道：tokenize -> parse -> elaborate -> check */
bool run_pipeline(const char* source, ConvStrategy strategy, PipelineResult* result);

/* 运行完整管道（带类型检查）：tokenize -> parse -> elaborate -> typecheck */
bool run_pipeline_with_check(const char* source, ConvStrategy strategy, PipelineResult* result);

/* 从文件运行管道 */
bool check_file(const char* path, ConvStrategy strategy, PipelineResult* result);

/* 释放管道结果 */
void pipeline_result_free(PipelineResult* result);

#endif /* PIPELINE_H */
