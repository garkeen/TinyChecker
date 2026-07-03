#include "cdecl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * CDecl 释放函数
 * ============================================================ */

static void free_term(Term* term) {
    /* Term 使用 arena 或全局分配，这里不释放 */
    (void)term;
}

void cdecl_free(CDecl* decl) {
    if (decl == NULL) return;
    
    switch (decl->kind) {
        case CDECL_DEFINITION:
            free(decl->def.name);
            free(decl->def.kind);
            break;
        
        case CDECL_TYPE_CTOR:
            free(decl->type_ctor.name);
            free(decl->type_ctor.kind);
            free(decl->type_ctor.family_kind);
            for (uint32_t i = 0; i < decl->type_ctor.param_count; i++) {
                free(decl->type_ctor.param_names[i]);
            }
            free(decl->type_ctor.param_names);
            free(decl->type_ctor.param_types);
            for (uint32_t i = 0; i < decl->type_ctor.index_count; i++) {
                free(decl->type_ctor.index_names[i]);
            }
            free(decl->type_ctor.index_names);
            free(decl->type_ctor.index_types);
            for (uint32_t i = 0; i < decl->type_ctor.constructor_count; i++) {
                free(decl->type_ctor.constructor_names[i]);
            }
            free(decl->type_ctor.constructor_names);
            break;
        
        case CDECL_DATA_CTOR:
            free(decl->data_ctor.name);
            free(decl->data_ctor.owner);
            free(decl->data_ctor.kind);
            for (uint32_t i = 0; i < decl->data_ctor.param_count; i++) {
                free(decl->data_ctor.param_names[i]);
            }
            free(decl->data_ctor.param_names);
            free(decl->data_ctor.param_types);
            free(decl->data_ctor.target_args);
            for (uint32_t i = 0; i < decl->data_ctor.recursive_field_count; i++) {
                CRecursiveFieldDecl* rec = &decl->data_ctor.recursive_fields[i];
                for (uint32_t j = 0; j < rec->ho_count; j++) {
                    free(rec->ho_names[j]);
                }
                free(rec->ho_names);
                free(rec->ho_types);
                free(rec->recursive_target_args);
            }
            free(decl->data_ctor.recursive_fields);
            break;
        
        case CDECL_ELIMINATOR:
            free(decl->eliminator.name);
            free(decl->eliminator.owner);
            free(decl->eliminator.kind);
            for (uint32_t i = 0; i < decl->eliminator.branch_count; i++) {
                free(decl->eliminator.branch_order[i]);
            }
            free(decl->eliminator.branch_order);
            break;
    }
}

/* ============================================================
 * CDecl 打印函数（调试用）
 * ============================================================ */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void cdecl_print(const CDecl* decl) {
    if (decl == NULL) {
        printf("NULL\n");
        return;
    }
    
    switch (decl->kind) {
        case CDECL_DEFINITION:
            printf("CDefinition %s: kind=%s\n", decl->def.name, decl->def.kind);
            break;
        
        case CDECL_TYPE_CTOR:
            printf("CTypeCtorDecl %s: kind=%s, family_kind=%s, params=%u, indices=%u, ctors=%u\n",
                   decl->type_ctor.name, decl->type_ctor.kind, decl->type_ctor.family_kind,
                   decl->type_ctor.param_count, decl->type_ctor.index_count,
                   decl->type_ctor.constructor_count);
            break;
        
        case CDECL_DATA_CTOR:
            printf("CDataCtorDecl %s: owner=%s, kind=%s, params=%u, rec_fields=%u\n",
                   decl->data_ctor.name, decl->data_ctor.owner, decl->data_ctor.kind,
                   decl->data_ctor.param_count, decl->data_ctor.recursive_field_count);
            break;
        
        case CDECL_ELIMINATOR:
            printf("CEliminatorDecl %s: owner=%s, kind=%s, branches=%u\n",
                   decl->eliminator.name, decl->eliminator.owner, decl->eliminator.kind,
                   decl->eliminator.branch_count);
            break;
    }
}
