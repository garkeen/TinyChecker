#include "pipeline.h"
#include "restricted.h"
#include "cdecl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 辅助函数：从 CDecl 注册到 GlobalContext
 * ============================================================ */

/* 辅助函数：复制字符串数组 */
static char** dup_str_array(char** src, uint32_t count) {
    if (count == 0 || src == NULL) return NULL;
    char** dst = (char**)malloc(count * sizeof(char*));
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = src[i] ? strdup(src[i]) : NULL;
    }
    return dst;
}

/* 辅助函数：复制 Term 指针数组 */
static const Term** dup_term_array(const Term** src, uint32_t count) {
    if (count == 0 || src == NULL) return NULL;
    const Term** dst = (const Term**)malloc(count * sizeof(const Term*));
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = src[i];  /* Term 本身不复制，只复制指针 */
    }
    return dst;
}

static void register_cdecl(GlobalContext* ctx, CDecl* cdecl, ConvStrategy strategy) {
    switch (cdecl->kind) {
        case CDECL_DEFINITION: {
            GlobalKind gkind = GLOBAL_VAR;
            if (strcmp(cdecl->def.kind, "fun") == 0) gkind = GLOBAL_FUN;
            else if (strcmp(cdecl->def.kind, "theorem") == 0) gkind = GLOBAL_THEOREM;
            else if (strcmp(cdecl->def.kind, "claim") == 0) gkind = GLOBAL_CLAIM;
            else if (strcmp(cdecl->def.kind, "axiom") == 0) gkind = GLOBAL_AXIOM;
            
            global_ctx_register(ctx, (GlobalEntry){
                .name = strdup(cdecl->def.name),
                .typ = cdecl->def.typ,
                .value = cdecl->def.value,
                .kind = gkind
            });
            
            /* 类型检查：检查值是否符合类型 */
            if (cdecl->def.value != NULL) {
                TypeChecker tc;
                typechecker_init(&tc, ctx, strategy);
                Context tctx;
                ctx_init(&tctx);
                bool ok = typechecker_check(&tc, cdecl->def.value, cdecl->def.typ, &tctx);
                if (!ok) {
                    printf("  Type check failed for %s\n", cdecl->def.name);
                }
            }
            break;
        }
        case CDECL_TYPE_CTOR: {
            global_ctx_register(ctx, (GlobalEntry){
                .name = strdup(cdecl->type_ctor.name),
                .typ = cdecl->type_ctor.typ,
                .value = NULL,
                .kind = GLOBAL_TYPE_CTOR
            });
            
            InductiveInfo info = {
                .name = strdup(cdecl->type_ctor.name),
                .kind = strdup(cdecl->type_ctor.family_kind),
                .param_count = cdecl->type_ctor.param_count,
                .index_count = cdecl->type_ctor.index_count,
                .param_names = dup_str_array(cdecl->type_ctor.param_names, cdecl->type_ctor.param_count),
                .param_types = dup_term_array((const Term**)cdecl->type_ctor.param_types, cdecl->type_ctor.param_count),
                .index_names = dup_str_array(cdecl->type_ctor.index_names, cdecl->type_ctor.index_count),
                .index_types = dup_term_array((const Term**)cdecl->type_ctor.index_types, cdecl->type_ctor.index_count),
                .constructor_names = dup_str_array(cdecl->type_ctor.constructor_names, cdecl->type_ctor.constructor_count),
                .constructor_count = cdecl->type_ctor.constructor_count,
                .eliminator_name = NULL
            };
            global_ctx_register_inductive(ctx, info);
            break;
        }
        case CDECL_DATA_CTOR: {
            global_ctx_register(ctx, (GlobalEntry){
                .name = strdup(cdecl->data_ctor.name),
                .typ = cdecl->data_ctor.typ,
                .value = NULL,
                .kind = GLOBAL_DATA_CTOR
            });
            
            RecursiveField* recursive_fields = NULL;
            if (cdecl->data_ctor.recursive_field_count > 0) {
                recursive_fields = (RecursiveField*)malloc(cdecl->data_ctor.recursive_field_count * sizeof(RecursiveField));
                for (uint32_t i = 0; i < cdecl->data_ctor.recursive_field_count; i++) {
                    CRecursiveFieldDecl* crec = &cdecl->data_ctor.recursive_fields[i];
                    recursive_fields[i].field_position = crec->field_position;
                    recursive_fields[i].recursive_kind = "direct";
                    recursive_fields[i].ho_telescope_names = dup_str_array(crec->ho_names, crec->ho_count);
                    recursive_fields[i].ho_telescope_types = dup_term_array((const Term**)crec->ho_types, crec->ho_count);
                    recursive_fields[i].ho_telescope_count = crec->ho_count;
                    recursive_fields[i].recursive_target_args = dup_term_array((const Term**)crec->recursive_target_args, crec->recursive_target_arg_count);
                    recursive_fields[i].recursive_target_arg_count = crec->recursive_target_arg_count;
                }
            }
            
            ConstructorInfo ctor_info = {
                .name = strdup(cdecl->data_ctor.name),
                .owner = strdup(cdecl->data_ctor.owner),
                .typ = cdecl->data_ctor.typ,
                .param_count = cdecl->data_ctor.spec_param_count,
                .field_names = dup_str_array(cdecl->data_ctor.param_names, cdecl->data_ctor.param_count),
                .field_types = dup_term_array((const Term**)cdecl->data_ctor.param_types, cdecl->data_ctor.param_count),
                .field_count = cdecl->data_ctor.param_count - cdecl->data_ctor.spec_param_count,
                .recursive_fields = recursive_fields,
                .recursive_field_count = cdecl->data_ctor.recursive_field_count,
                .target_args = dup_term_array((const Term**)cdecl->data_ctor.target_args, cdecl->data_ctor.target_arg_count),
                .target_arg_count = cdecl->data_ctor.target_arg_count
            };
            global_ctx_register_constructor(ctx, ctor_info);
            break;
        }
        case CDECL_ELIMINATOR: {
            global_ctx_register(ctx, (GlobalEntry){
                .name = strdup(cdecl->eliminator.name),
                .typ = cdecl->eliminator.typ,
                .value = NULL,
                .kind = GLOBAL_ELIMINATOR
            });
            
            EliminatorInfo elim_info = {
                .name = strdup(cdecl->eliminator.name),
                .owner = strdup(cdecl->eliminator.owner),
                .typ = cdecl->eliminator.typ,
                .branch_order = dup_str_array(cdecl->eliminator.branch_order, cdecl->eliminator.branch_count),
                .branch_count = cdecl->eliminator.branch_count,
                .kind = strdup(cdecl->eliminator.kind)
            };
            global_ctx_register_eliminator(ctx, elim_info);
            break;
        }
    }
}

/* ============================================================
 * 运行完整管道
 * ============================================================ */

bool run_pipeline(const char* source, ConvStrategy strategy, PipelineResult* result) {
    result->success = false;
    result->error_msg[0] = '\0';
    
    /* 1. 词法分析 */
    lexer_init(&result->lexer, source);
    if (!lexer_tokenize(&result->lexer)) {
        snprintf(result->error_msg, sizeof(result->error_msg), "lexer error: %s", result->lexer.error_msg);
        return false;
    }
    
    /* 2. 语法分析 */
    parser_init(&result->parser, result->lexer.tokens, result->lexer.token_count);
    if (!parser_parse(&result->parser)) {
        snprintf(result->error_msg, sizeof(result->error_msg), "parser error: %s", result->parser.error_msg);
        return false;
    }
    
    /* 3. 精细化 */
    elaborator_init(&result->elaborator);
    global_ctx_init(&result->global_ctx);
    
    /* 精细化每个声明 */
    for (uint32_t i = 0; i < result->parser.decl_count; i++) {
        Decl* decl = result->parser.decls[i];
        uint32_t rdecl_count = 0;
        RDecl* rdecls = elab_decl(&result->elaborator, decl, &rdecl_count);
        
        /* 将 RDecl 转换为 CDecl */
        for (uint32_t j = 0; j < rdecl_count; j++) {
            RDecl* rd = &rdecls[j];
            uint32_t cdecl_count = 0;
            CDecl* cdecls = restricted_decl_to_core(rd, &cdecl_count);
            
            /* 注册 CDecl 到全局上下文 */
            for (uint32_t k = 0; k < cdecl_count; k++) {
                register_cdecl(&result->global_ctx, &cdecls[k], strategy);
            }
            
            /* 释放 CDecl */
            for (uint32_t k = 0; k < cdecl_count; k++) {
                cdecl_free(&cdecls[k]);
            }
            free(cdecls);
        }
        
        /* 释放 RDecl */
        free(rdecls);
    }
    
    result->success = true;
    return true;
}

/* ============================================================
 * 运行完整管道（带类型检查）
 * ============================================================ */

bool run_pipeline_with_check(const char* source, ConvStrategy strategy, PipelineResult* result) {
    result->success = false;
    result->error_msg[0] = '\0';
    
    /* 1. 词法分析 */
    lexer_init(&result->lexer, source);
    if (!lexer_tokenize(&result->lexer)) {
        snprintf(result->error_msg, sizeof(result->error_msg), "lexer error: %s", result->lexer.error_msg);
        return false;
    }
    
    /* 2. 语法分析 */
    parser_init(&result->parser, result->lexer.tokens, result->lexer.token_count);
    if (!parser_parse(&result->parser)) {
        snprintf(result->error_msg, sizeof(result->error_msg), "parser error: %s", result->parser.error_msg);
        return false;
    }
    
    /* 3. 精细化 */
    elaborator_init(&result->elaborator);
    global_ctx_init(&result->global_ctx);
    
    /* 精细化每个声明 */
    for (uint32_t i = 0; i < result->parser.decl_count; i++) {
        Decl* decl = result->parser.decls[i];
        uint32_t rdecl_count = 0;
        RDecl* rdecls = elab_decl(&result->elaborator, decl, &rdecl_count);
        
        /* 将 RDecl 转换为 CDecl */
        for (uint32_t j = 0; j < rdecl_count; j++) {
            RDecl* rd = &rdecls[j];
            uint32_t cdecl_count = 0;
            CDecl* cdecls = restricted_decl_to_core(rd, &cdecl_count);
            
            /* 注册 CDecl 到全局上下文 */
            for (uint32_t k = 0; k < cdecl_count; k++) {
                register_cdecl(&result->global_ctx, &cdecls[k], strategy);
            }
            
            /* 释放 CDecl */
            for (uint32_t k = 0; k < cdecl_count; k++) {
                cdecl_free(&cdecls[k]);
            }
            free(cdecls);
        }
        
        /* 释放 RDecl */
        free(rdecls);
    }
    
    /* 4. 类型检查 */
    TypeChecker tc;
    typechecker_init(&tc, &result->global_ctx, strategy);
    
    /* 对每个有值的定义进行类型检查 */
    for (uint32_t i = 0; i < result->global_ctx.global_count; i++) {
        GlobalEntry* entry = &result->global_ctx.globals[i];
        if (entry->value != NULL) {
            Context ctx;
            ctx_init(&ctx);
            
            /* 检查类型是否有效 */
            if (!typechecker_check(&tc, entry->typ, term_new_type(), &ctx)) {
                snprintf(result->error_msg, sizeof(result->error_msg), 
                         "type error in %s: invalid type", entry->name);
                ctx_free(&ctx);
                return false;
            }
            
            /* 检查值是否符合类型 */
            if (!typechecker_check(&tc, entry->value, entry->typ, &ctx)) {
                snprintf(result->error_msg, sizeof(result->error_msg), 
                         "type error in %s: value does not match type", entry->name);
                ctx_free(&ctx);
                return false;
            }
            
            ctx_free(&ctx);
        }
    }
    
    result->success = true;
    return true;
}

/* ============================================================
 * 从文件运行管道
 * ============================================================ */

bool check_file(const char* path, ConvStrategy strategy, PipelineResult* result) {
    FILE* f = fopen(path, "r");
    if (!f) {
        snprintf(result->error_msg, sizeof(result->error_msg), "cannot open file: %s", path);
        result->success = false;
        return false;
    }
    
    /* 获取文件大小 */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* 读取文件内容 */
    char* source = (char*)malloc(size + 1);
    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);
    
    bool ok = run_pipeline(source, strategy, result);
    free(source);
    return ok;
}

/* ============================================================
 * 释放管道结果
 * ============================================================ */

void pipeline_result_free(PipelineResult* result) {
    /* lexer 和 parser 的内存由其内部管理 */
    elaborator_free(&result->elaborator);
    /* global_ctx 的内存由其内部管理 */
}
