#ifndef CDECL_H
#define CDECL_H

#include "term.h"

/* ============================================================
 * CDecl - 核心声明层
 * ============================================================ */

typedef enum {
    CDECL_DEFINITION,
    CDECL_TYPE_CTOR,
    CDECL_DATA_CTOR,
    CDECL_ELIMINATOR,
} CDeclKind;

/* 递归字段声明 */
typedef struct {
    uint32_t field_position;
    /* ho_telescope */
    char** ho_names;
    Term** ho_types;
    uint32_t ho_count;
    /* recursive_target_args */
    Term** recursive_target_args;
    uint32_t recursive_target_arg_count;
} CRecursiveFieldDecl;

/* 定义声明 */
typedef struct {
    char* name;
    Term* typ;
    Term* value;  /* 可能为 NULL */
    char* kind;
} CDefinition;

/* 类型构造器声明 */
typedef struct {
    char* name;
    /* param_telescope */
    char** param_names;
    Term** param_types;
    uint32_t param_count;
    /* index_telescope */
    char** index_names;
    Term** index_types;
    uint32_t index_count;
    Term* typ;
    char* kind;
    char* family_kind;
    char** constructor_names;
    uint32_t constructor_count;
} CTypeCtorDecl;

/* 数据构造器声明 */
typedef struct {
    char* name;
    char* owner;
    Term* typ;
    char* kind;
    /* constructor_parameter_list */
    char** param_names;
    Term** param_types;
    uint32_t param_count;      /* 总参数数（type_params + fields） */
    uint32_t spec_param_count; /* 类型参数数（只有 type_params） */
    /* target_args */
    Term** target_args;
    uint32_t target_arg_count;
    /* recursive_fields */
    CRecursiveFieldDecl* recursive_fields;
    uint32_t recursive_field_count;
} CDataCtorDecl;

/* 消去子声明 */
typedef struct {
    char* name;
    char* owner;
    Term* motive_type;
    Term* typ;
    char* kind;
    char** branch_order;
    uint32_t branch_count;
} CEliminatorDecl;

/* CDecl 联合类型 */
typedef struct {
    CDeclKind kind;
    union {
        CDefinition def;
        CTypeCtorDecl type_ctor;
        CDataCtorDecl data_ctor;
        CEliminatorDecl eliminator;
    };
} CDecl;

/* ============================================================
 * CDecl 操作函数
 * ============================================================ */

/* 释放 CDecl */
void cdecl_free(CDecl* decl);

/* 打印 CDecl（调试用） */
void cdecl_print(const CDecl* decl);

#endif /* CDECL_H */
