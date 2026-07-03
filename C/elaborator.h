#ifndef ELABORATOR_H
#define ELABORATOR_H

#include "surface.h"
#include "restricted.h"
#include <stdbool.h>

/* ============================================================
 * ElabLocalContext
 * ============================================================ */

typedef struct {
    char** names;
    uint32_t count;
} ElabLocalContext;

void elab_local_init(ElabLocalContext* ctx);
void elab_local_free(ElabLocalContext* ctx);
void elab_local_extend(ElabLocalContext* ctx, const char* name);

/* ============================================================
 * InductiveSpec
 * ============================================================ */

typedef struct ElabRecFieldInfo {
    uint32_t field_position;
    char* recursive_kind;
    /* ho_telescope */
    char** ho_names;
    Expr** ho_types;
    uint32_t ho_count;
    /* target_index_exprs */
    Expr** target_index_exprs;
    uint32_t target_index_count;
} ElabRecFieldInfo;

typedef struct ElabCtorInfo {
    char* name;
    /* fields */
    char** field_names;
    Expr** field_types;
    uint32_t field_count;
    /* recursive_fields */
    ElabRecFieldInfo* recursive_fields;
    uint32_t recursive_field_count;
    /* target_index_exprs */
    Expr** target_index_exprs;
    uint32_t target_index_count;
} ElabCtorInfo;

typedef struct InductiveSpec {
    char* name;
    char* kind;
    /* param_telescope */
    char** param_names;
    Expr** param_types;
    uint32_t param_count;
    /* index_telescope */
    char** index_names;
    Expr** index_types;
    uint32_t index_count;
    /* constructors */
    char** ctor_names;
    ElabCtorInfo* ctors;
    uint32_t ctor_count;
    char* eliminator_name;
} InductiveSpec;

/* ============================================================
 * ElabState
 * ============================================================ */

#define MAX_GLOBAL_TAGS 1024
#define MAX_INDUCTIVES 128

typedef struct {
    /* global_tags */
    char* tag_names[MAX_GLOBAL_TAGS];
    char* tag_values[MAX_GLOBAL_TAGS];
    uint32_t tag_count;
    /* inductives */
    InductiveSpec* inductives[MAX_INDUCTIVES];
    uint32_t inductive_count;
    /* counters */
    uint32_t example_counter;
    uint32_t surface_counter;
    /* fresh name counters */
    char* fresh_prefixes[64];
    uint32_t fresh_counts[64];
    uint32_t fresh_count;
} ElabState;

void elab_state_init(ElabState* state);
void elab_state_free(ElabState* state);
const char* elab_state_fresh(ElabState* state, const char* prefix);
const char* elab_state_fresh_example_name(ElabState* state);
void elab_state_register_global(ElabState* state, const char* name, const char* tag);
bool elab_state_is_defined(ElabState* state, const char* name);
const char* elab_state_resolve_dot(ElabState* state, const char* text);

/* ============================================================
 * Elaborator
 * ============================================================ */

typedef struct {
    ElabState state;
} Elaborator;

void elaborator_init(Elaborator* elab);
void elaborator_free(Elaborator* elab);

/* 精细化声明 */
RDecl* elab_decl(Elaborator* elab, Decl* decl, uint32_t* out_count);

/* 精细化表达式 */
Expr* elab_lower_expr(Elaborator* elab, Expr* expr, ElabLocalContext* local);

/* 精细化整个程序 */
RDecl* elab_program(Elaborator* elab, Decl** decls, uint32_t decl_count, uint32_t* out_count);

#endif /* ELABORATOR_H */
