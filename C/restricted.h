#ifndef RESTRICTED_H
#define RESTRICTED_H

#include "surface.h"
#include "term.h"
#include "cdecl.h"
#include <stdbool.h>

/* ============================================================
 * 受限声明类型
 * ============================================================ */

typedef struct RDefinition {
    char* name;
    Expr* typ;
    Expr* value;  /* 可能为 NULL */
    char* kind;
} RDefinition;

typedef struct RTypeCtorDecl {
    char* name;
    /* param_telescope */
    char** param_names;
    Expr** param_types;
    uint32_t param_count;
    /* index_telescope */
    char** index_names;
    Expr** index_types;
    uint32_t index_count;
    Expr* typ;
    char* kind;
    char* family_kind;
    char** constructor_names;
    uint32_t constructor_count;
} RTypeCtorDecl;

typedef struct RRecFieldInfo {
    uint32_t field_position;
    char* recursive_kind;
    /* ho_telescope */
    char** ho_names;
    Expr** ho_types;
    uint32_t ho_count;
    /* target_index_exprs */
    Expr** target_index_exprs;
    uint32_t target_index_count;
} RRecFieldInfo;

typedef struct RDataCtorDecl {
    char* name;
    char* owner;
    Expr* typ;
    char* kind;
    /* constructor_parameter_list */
    char** param_names;
    Expr** param_types;
    uint32_t param_count;      /* 总参数数（type_params + fields） */
    uint32_t spec_param_count; /* 类型参数数（只有 type_params） */
    /* target_index_exprs */
    Expr** target_index_exprs;
    uint32_t target_index_count;
    /* recursive_fields */
    RRecFieldInfo* recursive_fields;
    uint32_t recursive_field_count;
} RDataCtorDecl;

typedef struct REliminatorDecl {
    char* name;
    char* owner;
    Expr* motive_type;
    Expr* typ;
    char* kind;
    char** branch_order;
    uint32_t branch_count;
} REliminatorDecl;

/* ============================================================
 * 受限声明联合类型
 * ============================================================ */

typedef enum {
    RDECL_DEFINITION,
    RDECL_TYPE_CTOR,
    RDECL_DATA_CTOR,
    RDECL_ELIMINATOR,
} RDeclKind;

typedef struct {
    RDeclKind kind;
    union {
        RDefinition def;
        RTypeCtorDecl type_ctor;
        RDataCtorDecl data_ctor;
        REliminatorDecl eliminator;
    };
} RDecl;

/* ============================================================
 * 受限表达式操作
 * ============================================================ */

/* 在局部名列表中查找 de Bruijn 索引 */
int lookup_local(const char** local, uint32_t count, const char* name);

/* 从 telescope 构建 Pi 表达式链 */
Expr* fold_pi_expr(char** names, Expr** types, uint32_t count, Expr* body);

/* 从 telescope 构建 Lambda 表达式链 */
Expr* fold_lam_expr(char** names, Expr** types, uint32_t count, Expr* body);

/* 拆解 Pi 表达式为 telescope */
typedef struct {
    char** names;
    Expr** types;
    uint32_t count;
    Expr* body;
} PiTelescope;

PiTelescope split_pi_expr(Expr* expr);
void pi_telescope_free(PiTelescope* tel);

/* 检查表达式是否包含特定原子名 */
bool contains_atom(Expr* expr, const char* text);

/* 计算表达式的自由变量集 */
typedef struct {
    char** names;
    uint32_t count;
} FreeVarSet;

FreeVarSet free_vars(Expr* expr);
void free_var_set_free(FreeVarSet* set);

/* capture-avoiding substitution */
typedef char* (*FreshNameFn)(const char* prefix, void* ctx);

Expr* substitute_expr(Expr* expr, 
                      const char** keys, Expr** values, uint32_t mapping_count,
                      FreshNameFn fresh_name, void* fresh_ctx);

/* 受限表达式到核心 term 的转换 */
Term* restricted_expr_to_core(Expr* expr, const char** local, uint32_t local_count);

/* 受限声明到核心声明的转换 */
CDecl* restricted_decl_to_core(RDecl* rdecl, uint32_t* out_count);

/* 获取 Pi 类型的最终结果类型 */
Expr* final_result_of_ctor_type(Expr* expr);

#endif /* RESTRICTED_H */
