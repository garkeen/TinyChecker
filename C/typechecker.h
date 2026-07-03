#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "term.h"
#include "context.h"
#include "runtime.h"
#include "reducer.h"
#include <stdbool.h>

/* ============================================================
 * 类型检查器
 * ============================================================ */

typedef struct {
    GlobalContext* global_ctx;
    ConvStrategy strategy;
} TypeChecker;

/* 初始化类型检查器 */
void typechecker_init(TypeChecker* tc, GlobalContext* ctx, ConvStrategy strategy);

/* 检查整个程序 */
bool typechecker_check_program(TypeChecker* tc, const Term** decls, size_t count);

/* 推导项的类型 */
Term* typechecker_infer(TypeChecker* tc, const Term* term, Context* ctx);

/* 检查项是否符合期望的类型 */
bool typechecker_check(TypeChecker* tc, const Term* term, const Term* expected, Context* ctx);

/* 检查两个类型是否相等 */
bool typechecker_convert(TypeChecker* tc, const Term* lhs, const Term* rhs);

/* 注册全局定义 */
bool typechecker_register_definition(TypeChecker* tc, const char* name, const Term* typ, const Term* value, const char* kind);

/* 注册类型构造器 */
bool typechecker_register_type_ctor(TypeChecker* tc, const char* name, const char* kind,
    char** param_names, const Term** param_types, uint32_t param_count,
    char** index_names, const Term** index_types, uint32_t index_count,
    const char** ctor_names, uint32_t ctor_count);

/* 注册数据构造器 */
bool typechecker_register_data_ctor(TypeChecker* tc, const char* name, const char* owner,
    const Term* typ, uint32_t param_count,
    char** field_names, const Term** field_types, uint32_t field_count);

/* 注册消去子 */
bool typechecker_register_eliminator(TypeChecker* tc, const char* name, const char* owner,
    const Term* typ, const char** branch_order, uint32_t branch_count, const char* kind);

#endif /* TYPECHECKER_H */
