#ifndef RUNTIME_H
#define RUNTIME_H

#include "term.h"
#include <stdbool.h>

/* ============================================================
 * 全局上下文条目
 * ============================================================ */

typedef enum {
    GLOBAL_VAR,
    GLOBAL_FUN,
    GLOBAL_THEOREM,
    GLOBAL_CLAIM,
    GLOBAL_AXIOM,
    GLOBAL_TYPE_CTOR,
    GLOBAL_DATA_CTOR,
    GLOBAL_ELIMINATOR,
    GLOBAL_PROJECTION,
} GlobalKind;

typedef struct GlobalEntry {
    const char* name;
    const Term* typ;
    const Term* value;      /* 可能为 NULL（如 axiom） */
    GlobalKind kind;
} GlobalEntry;

/* ============================================================
 * 归纳类型信息
 * ============================================================ */

typedef struct {
    const char* name;
    const char* kind;       /* "inductive", "sum", "product" */
    uint32_t param_count;
    uint32_t index_count;
    /* 参数 telescope */
    char** param_names;
    const Term** param_types;
    /* 索引 telescope */
    char** index_names;
    const Term** index_types;
    /* 构造器 */
    const char** constructor_names;
    uint32_t constructor_count;
    const char* eliminator_name;
} InductiveInfo;

/* ============================================================
 * 构造器信息
 * ============================================================ */

typedef struct {
    uint32_t field_position;
    const char* recursive_kind;  /* "direct" 或 "higher" */
    /* 高阶递归的 telescope */
    char** ho_telescope_names;
    const Term** ho_telescope_types;
    uint32_t ho_telescope_count;
    /* 递归目标的参数 */
    const Term** recursive_target_args;
    uint32_t recursive_target_arg_count;
} RecursiveField;

typedef struct ConstructorInfo {
    const char* name;
    const char* owner;          /* 所属归纳类型名 */
    const Term* typ;
    uint32_t param_count;
    /* 构造器的字段列表 */
    char** field_names;
    const Term** field_types;
    uint32_t field_count;
    /* 递归字段 */
    RecursiveField* recursive_fields;
    uint32_t recursive_field_count;
    /* 目标索引参数 */
    const Term** target_args;
    uint32_t target_arg_count;
} ConstructorInfo;

/* ============================================================
 * 消去子信息
 * ============================================================ */

typedef struct EliminatorInfo {
    const char* name;
    const char* owner;
    const Term* typ;
    const char** branch_order;  /* 分支名顺序 */
    uint32_t branch_count;
    const char* kind;           /* "rec" 或 "elim" */
} EliminatorInfo;

/* ============================================================
 * 全局上下文
 * ============================================================ */

#define MAX_GLOBALS 1024
#define MAX_INDUCTIVES 128
#define MAX_CONSTRUCTORS 256
#define MAX_ELIMINATORS 128

typedef struct {
    GlobalEntry globals[MAX_GLOBALS];
    uint32_t global_count;
    
    InductiveInfo inductives[MAX_INDUCTIVES];
    uint32_t inductive_count;
    
    ConstructorInfo constructors[MAX_CONSTRUCTORS];
    uint32_t constructor_count;
    
    EliminatorInfo eliminators[MAX_ELIMINATORS];
    uint32_t eliminator_count;
} GlobalContext;

/* 初始化全局上下文 */
void global_ctx_init(GlobalContext* ctx);

/* 注册全局条目 */
bool global_ctx_register(GlobalContext* ctx, GlobalEntry entry);

/* 查找全局条目 */
GlobalEntry* global_ctx_lookup(GlobalContext* ctx, const char* name);

/* 注册归纳类型 */
bool global_ctx_register_inductive(GlobalContext* ctx, InductiveInfo info);

/* 查找归纳类型 */
InductiveInfo* global_ctx_lookup_inductive(GlobalContext* ctx, const char* name);

/* 注册构造器 */
bool global_ctx_register_constructor(GlobalContext* ctx, ConstructorInfo info);

/* 查找构造器 */
ConstructorInfo* global_ctx_lookup_constructor(GlobalContext* ctx, const char* name);

/* 注册消去子 */
bool global_ctx_register_eliminator(GlobalContext* ctx, EliminatorInfo info);

/* 查找消去子 */
EliminatorInfo* global_ctx_lookup_eliminator(GlobalContext* ctx, const char* name);

#endif /* RUNTIME_H */
