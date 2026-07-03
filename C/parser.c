#include "parser.h"
#include "surface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

static const Token* current_token(Parser* parser) {
    if (parser->pos >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1];
    }
    return &parser->tokens[parser->pos];
}

static const Token* peek_token(Parser* parser, uint32_t offset) {
    uint32_t index = parser->pos + offset;
    if (index >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1];
    }
    return &parser->tokens[index];
}

static bool at_token(Parser* parser, TokenKind kind) {
    return current_token(parser)->kind == kind;
}

static const Token* consume_token(Parser* parser, TokenKind kind) {
    const Token* token = current_token(parser);
    if (token->kind != kind) {
        char msg[256];
        snprintf(msg, sizeof(msg), "expected %s, got %s at %u:%u",
                 token_kind_name(kind), token_kind_name(token->kind),
                 token->row, token->col);
        snprintf(parser->error_msg, sizeof(parser->error_msg), "%s", msg);
        parser->has_error = true;
        return NULL;
    }
    parser->pos++;
    return token;
}

static bool maybe_token(Parser* parser, TokenKind kind) {
    if (at_token(parser, kind)) {
        parser->pos++;
        return true;
    }
    return false;
}

static void set_error(Parser* parser, const char* msg) {
    const Token* token = current_token(parser);
    snprintf(parser->error_msg, sizeof(parser->error_msg), "%s at %u:%u",
             msg, token->row, token->col);
    parser->has_error = true;
}

static char* copy_token_text(const Token* token) {
    char* text = (char*)malloc(token->text_len + 1);
    if (text) {
        memcpy(text, token->text, token->text_len);
        text[token->text_len] = '\0';
    }
    return text;
}

/* ============================================================
 * 表达式解析（前向声明）
 * ============================================================ */

static Expr* parse_expr(Parser* parser);
static Expr* parse_app(Parser* parser);
static Expr* parse_no_app(Parser* parser);
static Param* parse_param(Parser* parser);
static char* parse_bind_name(Parser* parser);

/* ============================================================
 * 原子和基础表达式
 * ============================================================ */

static Expr* parse_atom(Parser* parser) {
    const Token* token = current_token(parser);
    if (token->kind != TOKEN_IDENT && token->kind != TOKEN_DOT &&
        token->kind != TOKEN_TYPE && token->kind != TOKEN_PROP) {
        set_error(parser, "expected atom");
        return NULL;
    }
    parser->pos++;
    if (token->kind == TOKEN_IDENT && token->text_len == 1 && token->text[0] == '_') {
        set_error(parser, "'_' is not allowed as an expression");
        return NULL;
    }
    return expr_new_atom(copy_token_text(token));
}

static Expr* parse_paren_expr(Parser* parser) {
    consume_token(parser, TOKEN_LPAREN);
    Expr* expr = parse_expr(parser);
    consume_token(parser, TOKEN_RPAREN);
    return expr;
}

static Expr* parse_pi_expr(Parser* parser) {
    consume_token(parser, TOKEN_LPAREN);
    char* name = parse_bind_name(parser);
    consume_token(parser, TOKEN_COLON);
    Expr* domain = parse_expr(parser);
    consume_token(parser, TOKEN_RPAREN);
    consume_token(parser, TOKEN_ARROW);
    Expr* codomain = parse_expr(parser);
    return expr_new_pi(name, domain, codomain);
}

/* ============================================================
 * Lambda 表达式
 * ============================================================ */

static Expr* parse_lambda(Parser* parser) {
    consume_token(parser, TOKEN_LAMBDA);
    Param* param;
    if (at_token(parser, TOKEN_LPAREN)) {
        param = parse_param(parser);
    } else {
        const Token* name_token = consume_token(parser, TOKEN_IDENT);
        consume_token(parser, TOKEN_COLON);
        Expr* typ = parse_expr(parser);
        param = param_new(copy_token_text(name_token), typ);
    }
    consume_token(parser, TOKEN_DARROW);
    Expr* body = parse_expr(parser);
    return expr_new_lambda(param, body);
}

/* ============================================================
 * Let 表达式
 * ============================================================ */

static Expr* parse_let_expr(Parser* parser) {
    consume_token(parser, TOKEN_LET);
    const Token* name_token = consume_token(parser, TOKEN_IDENT);
    consume_token(parser, TOKEN_COLON);
    Expr* typ = parse_expr(parser);
    consume_token(parser, TOKEN_EQ);
    Expr* value = parse_expr(parser);
    consume_token(parser, TOKEN_IN);
    Expr* body = parse_expr(parser);
    return expr_new_let(copy_token_text(name_token), typ, value, body);
}

/* ============================================================
 * 等式表达式
 * ============================================================ */

static Expr* parse_eq_expr(Parser* parser) {
    consume_token(parser, TOKEN_LBRACKET);
    Expr* typ = parse_expr(parser);
    consume_token(parser, TOKEN_RBRACKET);
    Expr* lhs = parse_expr(parser);
    consume_token(parser, TOKEN_EQEQ);
    Expr* rhs = parse_expr(parser);
    return expr_new_eq(typ, lhs, rhs);
}

/* ============================================================
 * Match 表达式
 * ============================================================ */

static MatchBranch* parse_match_branch(Parser* parser) {
    consume_token(parser, TOKEN_BAR);
    const Token* ctor_token = consume_token(parser, TOKEN_IDENT);
    char* ctor = copy_token_text(ctor_token);

    char* fields[64];
    uint32_t field_count = 0;
    while (at_token(parser, TOKEN_IDENT)) {
        fields[field_count++] = parse_bind_name(parser);
    }

    char* ihs[64];
    uint32_t ih_count = 0;
    if (maybe_token(parser, TOKEN_LBRACKET)) {
        while (!at_token(parser, TOKEN_RBRACKET)) {
            ihs[ih_count++] = parse_bind_name(parser);
        }
        consume_token(parser, TOKEN_RBRACKET);
    }

    consume_token(parser, TOKEN_DARROW);
    Expr* body = parse_expr(parser);

    MatchBranch* branch = (MatchBranch*)calloc(1, sizeof(MatchBranch));
    branch->ctor = ctor;
    branch->field_count = field_count;
    branch->fields = (char**)malloc(field_count * sizeof(char*));
    memcpy(branch->fields, fields, field_count * sizeof(char*));
    branch->ih_count = ih_count;
    branch->ihs = (char**)malloc(ih_count * sizeof(char*));
    memcpy(branch->ihs, ihs, ih_count * sizeof(char*));
    branch->body = body;
    return branch;
}

static Expr* parse_match_expr(Parser* parser) {
    consume_token(parser, TOKEN_MATCH);
    Expr* scrutinee = parse_expr(parser);

    char* alias = NULL;
    if (maybe_token(parser, TOKEN_AS)) {
        alias = parse_bind_name(parser);
    }

    consume_token(parser, TOKEN_IN);
    Expr* family_expr = parse_expr(parser);

    /* 解析 family head 和 args */
    Expr* head = family_expr;
    Expr* args[64];
    uint32_t arg_count = 0;
    while (head->kind == EXPR_APP) {
        args[arg_count++] = head->app.arg;
        head = head->app.func;
    }
    /* 反转 args */
    for (uint32_t i = 0; i < arg_count / 2; i++) {
        Expr* tmp = args[i];
        args[i] = args[arg_count - 1 - i];
        args[arg_count - 1 - i] = tmp;
    }

    if (head->kind != EXPR_ATOM) {
        set_error(parser, "match family head must be an atom");
        return NULL;
    }
    char* inductive = strdup(head->atom.text);

    char* bind_names[64];
    uint32_t bind_name_count = 0;
    if (maybe_token(parser, TOKEN_BIND)) {
        while (at_token(parser, TOKEN_IDENT)) {
            bind_names[bind_name_count++] = parse_bind_name(parser);
        }
    }

    consume_token(parser, TOKEN_RETURN);
    Expr* motive_body = parse_expr(parser);
    consume_token(parser, TOKEN_WITH);

    MatchBranch* branches[64];
    uint32_t branch_count = 0;
    while (at_token(parser, TOKEN_BAR)) {
        branches[branch_count++] = parse_match_branch(parser);
    }
    consume_token(parser, TOKEN_END);

    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    e->kind = EXPR_MATCH;
    e->match.scrutinee = scrutinee;
    e->match.alias = alias;
    e->match.inductive = inductive;
    e->match.family_arg_count = arg_count;
    e->match.family_args = (Expr**)malloc(arg_count * sizeof(Expr*));
    memcpy(e->match.family_args, args, arg_count * sizeof(Expr*));
    e->match.bind_name_count = bind_name_count;
    e->match.bind_names = (char**)malloc(bind_name_count * sizeof(char*));
    memcpy(e->match.bind_names, bind_names, bind_name_count * sizeof(char*));
    e->match.motive_body = motive_body;
    e->match.branch_count = branch_count;
    e->match.branches = (MatchBranch**)malloc(branch_count * sizeof(MatchBranch*));
    memcpy(e->match.branches, branches, branch_count * sizeof(MatchBranch*));
    return e;
}

/* ============================================================
 * Case 表达式
 * ============================================================ */

static CaseBranch* parse_case_branch(Parser* parser) {
    consume_token(parser, TOKEN_BAR);
    const Token* ctor_token = consume_token(parser, TOKEN_IDENT);
    char* ctor = copy_token_text(ctor_token);

    char* fields[64];
    uint32_t field_count = 0;
    while (at_token(parser, TOKEN_IDENT)) {
        fields[field_count++] = parse_bind_name(parser);
    }

    consume_token(parser, TOKEN_DARROW);
    Expr* body = parse_expr(parser);

    CaseBranch* branch = (CaseBranch*)calloc(1, sizeof(CaseBranch));
    branch->ctor = ctor;
    branch->field_count = field_count;
    branch->fields = (char**)malloc(field_count * sizeof(char*));
    memcpy(branch->fields, fields, field_count * sizeof(char*));
    branch->body = body;
    return branch;
}

static Expr* parse_case_expr(Parser* parser) {
    consume_token(parser, TOKEN_CASE);
    Expr* scrutinee = parse_expr(parser);

    char* alias = NULL;
    if (maybe_token(parser, TOKEN_AS)) {
        alias = parse_bind_name(parser);
    }

    consume_token(parser, TOKEN_IN);
    Expr* family_expr = parse_expr(parser);

    Expr* head = family_expr;
    Expr* args[64];
    uint32_t arg_count = 0;
    while (head->kind == EXPR_APP) {
        args[arg_count++] = head->app.arg;
        head = head->app.func;
    }
    for (uint32_t i = 0; i < arg_count / 2; i++) {
        Expr* tmp = args[i];
        args[i] = args[arg_count - 1 - i];
        args[arg_count - 1 - i] = tmp;
    }

    if (head->kind != EXPR_ATOM) {
        set_error(parser, "case family head must be an atom");
        return NULL;
    }
    char* sum_type = strdup(head->atom.text);

    char* bind_names[64];
    uint32_t bind_name_count = 0;
    if (maybe_token(parser, TOKEN_BIND)) {
        while (at_token(parser, TOKEN_IDENT)) {
            bind_names[bind_name_count++] = parse_bind_name(parser);
        }
    }

    consume_token(parser, TOKEN_RETURN);
    Expr* motive_body = parse_expr(parser);
    consume_token(parser, TOKEN_OF);

    CaseBranch* branches[64];
    uint32_t branch_count = 0;
    while (at_token(parser, TOKEN_BAR)) {
        branches[branch_count++] = parse_case_branch(parser);
    }
    consume_token(parser, TOKEN_END);

    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    e->kind = EXPR_CASE;
    e->case_expr.scrutinee = scrutinee;
    e->case_expr.alias = alias;
    e->case_expr.sum_type = sum_type;
    e->case_expr.type_arg_count = arg_count;
    e->case_expr.type_args = (Expr**)malloc(arg_count * sizeof(Expr*));
    memcpy(e->case_expr.type_args, args, arg_count * sizeof(Expr*));
    e->case_expr.bind_name_count = bind_name_count;
    e->case_expr.bind_names = (char**)malloc(bind_name_count * sizeof(char*));
    memcpy(e->case_expr.bind_names, bind_names, bind_name_count * sizeof(char*));
    e->case_expr.motive_body = motive_body;
    e->case_expr.branch_count = branch_count;
    e->case_expr.branches = (CaseBranch**)malloc(branch_count * sizeof(CaseBranch*));
    memcpy(e->case_expr.branches, branches, branch_count * sizeof(CaseBranch*));
    return e;
}

/* ============================================================
 * Product 表达式
 * ============================================================ */

static Expr* parse_product_expr(Parser* parser) {
    const Token* name_token = consume_token(parser, TOKEN_IDENT);
    consume_token(parser, TOKEN_LANGLE);

    Expr* args[64];
    uint32_t arg_count = 0;
    if (!at_token(parser, TOKEN_RANGLE)) {
        args[arg_count++] = parse_expr(parser);
        while (at_token(parser, TOKEN_COMMA)) {
            consume_token(parser, TOKEN_COMMA);
            args[arg_count++] = parse_expr(parser);
        }
    }
    consume_token(parser, TOKEN_RANGLE);

    Expr* e = (Expr*)calloc(1, sizeof(Expr));
    e->kind = EXPR_PRODUCT;
    e->product.type_name = copy_token_text(name_token);
    e->product.arg_count = arg_count;
    e->product.args = (Expr**)malloc(arg_count * sizeof(Expr*));
    memcpy(e->product.args, args, arg_count * sizeof(Expr*));
    return e;
}

/* ============================================================
 * 不带应用的表达式
 * ============================================================ */

static Expr* parse_no_app(Parser* parser) {
    TokenKind kind = current_token(parser)->kind;

    switch (kind) {
        case TOKEN_LAMBDA:
            return parse_lambda(parser);
        case TOKEN_LET:
            return parse_let_expr(parser);
        case TOKEN_MATCH:
            return parse_match_expr(parser);
        case TOKEN_CASE:
            return parse_case_expr(parser);
        case TOKEN_LPAREN:
            if (peek_token(parser, 1)->kind == TOKEN_IDENT &&
                peek_token(parser, 2)->kind == TOKEN_COLON) {
                return parse_pi_expr(parser);
            }
            return parse_paren_expr(parser);
        case TOKEN_LBRACKET:
            return parse_eq_expr(parser);
        case TOKEN_IDENT:
            if (peek_token(parser, 1)->kind == TOKEN_LANGLE) {
                return parse_product_expr(parser);
            }
            return parse_atom(parser);
        case TOKEN_DOT:
        case TOKEN_TYPE:
        case TOKEN_PROP:
            return parse_atom(parser);
        default:
            set_error(parser, "unexpected token in expression");
            return NULL;
    }
}

/* ============================================================
 * 应用表达式
 * ============================================================ */

static Expr* parse_app(Parser* parser) {
    Expr* term = parse_no_app(parser);
    if (term == NULL) return NULL;

    while (1) {
        TokenKind kind = current_token(parser)->kind;
        if (kind == TOKEN_IDENT || kind == TOKEN_DOT || kind == TOKEN_TYPE ||
            kind == TOKEN_PROP || kind == TOKEN_LPAREN || kind == TOKEN_LBRACKET ||
            kind == TOKEN_LAMBDA || kind == TOKEN_LET || kind == TOKEN_MATCH ||
            kind == TOKEN_CASE) {
            Expr* arg = parse_no_app(parser);
            if (arg == NULL) return NULL;
            term = expr_new_app(term, arg);
        } else {
            break;
        }
    }
    return term;
}

/* ============================================================
 * 顶层表达式
 * ============================================================ */

static Expr* parse_expr(Parser* parser) {
    Expr* left = parse_app(parser);
    if (left == NULL) return NULL;

    if (at_token(parser, TOKEN_ARROW)) {
        consume_token(parser, TOKEN_ARROW);
        Expr* codomain = parse_expr(parser);
        return expr_new_arrow(left, codomain);
    }
    return left;
}

/* ============================================================
 * 参数和绑定名
 * ============================================================ */

static Param* parse_param(Parser* parser) {
    consume_token(parser, TOKEN_LPAREN);
    char* name = parse_bind_name(parser);
    consume_token(parser, TOKEN_COLON);
    Expr* typ = parse_expr(parser);
    consume_token(parser, TOKEN_RPAREN);
    return param_new(name, typ);
}

static char* parse_bind_name(Parser* parser) {
    const Token* token = consume_token(parser, TOKEN_IDENT);
    if (token->text_len == 1 && token->text[0] == '_') {
        return NULL;
    }
    return copy_token_text(token);
}

/* ============================================================
 * 声明解析
 * ============================================================ */

static Decl* parse_var_decl(Parser* parser, const char* kind) {
    const Token* name_token = consume_token(parser, TOKEN_IDENT);
    consume_token(parser, TOKEN_COLON);
    Expr* typ = parse_expr(parser);
    consume_token(parser, TOKEN_EQ);
    Expr* value = parse_expr(parser);
    consume_token(parser, TOKEN_SEMI);
    return decl_new_var(kind, copy_token_text(name_token), typ, value);
}

static Decl* parse_fun_decl(Parser* parser, const char* kind) {
    const Token* name_token = consume_token(parser, TOKEN_IDENT);

    Param* params[64];
    uint32_t param_count = 0;
    while (at_token(parser, TOKEN_LPAREN)) {
        params[param_count++] = parse_param(parser);
    }

    consume_token(parser, TOKEN_COLON);
    Expr* ret_type = parse_expr(parser);
    consume_token(parser, TOKEN_LBRACE);
    Expr* body = parse_expr(parser);
    consume_token(parser, TOKEN_RBRACE);
    maybe_token(parser, TOKEN_SEMI);

    Param** params_copy = (Param**)malloc(param_count * sizeof(Param*));
    memcpy(params_copy, params, param_count * sizeof(Param*));

    return decl_new_fun(kind, copy_token_text(name_token), params_copy, param_count, ret_type, body);
}

static CtorDecl** parse_ctor_block(Parser* parser, uint32_t* out_count) {
    consume_token(parser, TOKEN_LBRACE);

    CtorDecl* ctors[64];
    uint32_t count = 0;

    if (at_token(parser, TOKEN_RBRACE)) {
        consume_token(parser, TOKEN_RBRACE);
        *out_count = 0;
        return NULL;
    }

    consume_token(parser, TOKEN_BAR);
    while (!at_token(parser, TOKEN_RBRACE)) {
        const Token* name_token = consume_token(parser, TOKEN_IDENT);
        consume_token(parser, TOKEN_COLON);
        Expr* typ = parse_expr(parser);
        ctors[count++] = ctor_decl_new(copy_token_text(name_token), typ);

        if (at_token(parser, TOKEN_BAR)) {
            consume_token(parser, TOKEN_BAR);
        } else if (!at_token(parser, TOKEN_RBRACE)) {
            set_error(parser, "expected '|' or '}'");
            return NULL;
        }
    }
    consume_token(parser, TOKEN_RBRACE);

    CtorDecl** ctors_copy = (CtorDecl**)malloc(count * sizeof(CtorDecl*));
    memcpy(ctors_copy, ctors, count * sizeof(CtorDecl*));
    *out_count = count;
    return ctors_copy;
}

static Decl* parse_inductive_decl(Parser* parser, const char* kind) {
    const Token* name_token = consume_token(parser, TOKEN_IDENT);

    Param* params[64];
    uint32_t param_count = 0;
    if (strcmp(kind, "inductive") == 0) {
        while (at_token(parser, TOKEN_LPAREN)) {
            params[param_count++] = parse_param(parser);
        }
    }

    Expr* arity = NULL;
    if (maybe_token(parser, TOKEN_COLON)) {
        arity = parse_expr(parser);
    }

    uint32_t ctor_count = 0;
    CtorDecl** ctors = parse_ctor_block(parser, &ctor_count);
    maybe_token(parser, TOKEN_SEMI);

    Param** params_copy = NULL;
    if (param_count > 0) {
        params_copy = (Param**)malloc(param_count * sizeof(Param*));
        memcpy(params_copy, params, param_count * sizeof(Param*));
    }

    return decl_new_inductive(kind, copy_token_text(name_token), params_copy, param_count, arity, ctors, ctor_count);
}

static Decl* parse_product_decl(Parser* parser) {
    consume_token(parser, TOKEN_PRODUCT);
    const Token* name_token = consume_token(parser, TOKEN_IDENT);

    FieldDecl* fields[64];
    uint32_t field_count = 0;
    consume_token(parser, TOKEN_LBRACE);
    while (!at_token(parser, TOKEN_RBRACE)) {
        const Token* field_name = consume_token(parser, TOKEN_IDENT);
        consume_token(parser, TOKEN_COLON);
        Expr* field_ty = parse_expr(parser);
        FieldDecl* fd = (FieldDecl*)calloc(1, sizeof(FieldDecl));
        fd->name = copy_token_text(field_name);
        fd->typ = field_ty;
        fields[field_count++] = fd;

        if (at_token(parser, TOKEN_COMMA)) {
            consume_token(parser, TOKEN_COMMA);
        } else {
            break;
        }
    }
    consume_token(parser, TOKEN_RBRACE);
    maybe_token(parser, TOKEN_SEMI);

    FieldDecl** fields_copy = (FieldDecl**)malloc(field_count * sizeof(FieldDecl*));
    memcpy(fields_copy, fields, field_count * sizeof(FieldDecl*));

    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    d->kind = DECL_PRODUCT;
    d->product_decl.name = copy_token_text(name_token);
    d->product_decl.fields = fields_copy;
    d->product_decl.field_count = field_count;
    return d;
}

static Decl* parse_axiom_decl(Parser* parser) {
    if (!consume_token(parser, TOKEN_AXIOM)) return NULL;
    const Token* name_token = consume_token(parser, TOKEN_IDENT);
    if (!name_token) return NULL;
    if (!consume_token(parser, TOKEN_COLON)) return NULL;
    Expr* typ = parse_expr(parser);
    if (!typ) return NULL;
    if (!consume_token(parser, TOKEN_SEMI)) return NULL;
    return decl_new_axiom(copy_token_text(name_token), typ);
}

static Decl* parse_example_decl(Parser* parser) {
    consume_token(parser, TOKEN_EXAMPLE);
    consume_token(parser, TOKEN_COLON);
    Expr* typ = parse_expr(parser);
    consume_token(parser, TOKEN_EQ);
    Expr* value = parse_expr(parser);
    maybe_token(parser, TOKEN_SEMI);

    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    d->kind = DECL_EXAMPLE;
    d->example_decl.typ = typ;
    d->example_decl.value = value;
    return d;
}

static Decl* parse_equation_decl(Parser* parser) {
    const Token* name_token = consume_token(parser, TOKEN_IDENT);
    consume_token(parser, TOKEN_COLON);
    Expr* typ = parse_expr(parser);
    consume_token(parser, TOKEN_COMMA);
    const Token* name2_token = consume_token(parser, TOKEN_IDENT);

    if (name_token->text_len != name2_token->text_len ||
        memcmp(name_token->text, name2_token->text, name_token->text_len) != 0) {
        set_error(parser, "equation name mismatch");
        return NULL;
    }

    char* params[64];
    uint32_t param_count = 0;
    while (at_token(parser, TOKEN_IDENT)) {
        params[param_count++] = parse_bind_name(parser);
    }

    consume_token(parser, TOKEN_EQ);
    Expr* value = parse_expr(parser);
    maybe_token(parser, TOKEN_SEMI);

    char** params_copy = (char**)malloc(param_count * sizeof(char*));
    memcpy(params_copy, params, param_count * sizeof(char*));

    Decl* d = (Decl*)calloc(1, sizeof(Decl));
    d->kind = DECL_EQUATION;
    d->var_decl.kind_str = "equation";
    d->var_decl.name = copy_token_text(name_token);
    d->var_decl.typ = typ;
    d->var_decl.value = value;
    return d;
}

static Decl* parse_decl(Parser* parser) {
    TokenKind kind = current_token(parser)->kind;

    switch (kind) {
        case TOKEN_VAR:
            parser->pos++;
            return parse_var_decl(parser, "var");
        case TOKEN_CLAIM:
            parser->pos++;
            return parse_var_decl(parser, "claim");
        case TOKEN_FUN:
            parser->pos++;
            return parse_fun_decl(parser, "fun");
        case TOKEN_THEOREM:
            parser->pos++;
            return parse_fun_decl(parser, "theorem");
        case TOKEN_INDUCTIVE:
            parser->pos++;
            return parse_inductive_decl(parser, "inductive");
        case TOKEN_SUM:
            parser->pos++;
            return parse_inductive_decl(parser, "sum");
        case TOKEN_PRODUCT:
            return parse_product_decl(parser);
        case TOKEN_AXIOM:
            return parse_axiom_decl(parser);
        case TOKEN_EXAMPLE:
            return parse_example_decl(parser);
        case TOKEN_IDENT:
            return parse_equation_decl(parser);
        default: {
            char msg[256];
            snprintf(msg, sizeof(msg), "unexpected declaration: %s", token_kind_name(kind));
            set_error(parser, msg);
            return NULL;
        }
    }
}

/* ============================================================
 * 公共接口
 * ============================================================ */

void parser_init(Parser* parser, const Token* tokens, uint32_t token_count) {
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->pos = 0;
    parser->decl_count = 0;
    parser->has_error = false;
    parser->error_msg[0] = '\0';
}

bool parser_parse(Parser* parser) {
    while (!at_token(parser, TOKEN_EOF) && !parser->has_error) {
        Decl* decl = parse_decl(parser);
        if (decl == NULL) {
            return false;
        }
        parser->decls[parser->decl_count++] = decl;
        while (maybe_token(parser, TOKEN_SEMI)) {
            /* skip */
        }
    }
    return !parser->has_error;
}

uint32_t parser_decl_count(const Parser* parser) {
    return parser->decl_count;
}

Decl* parser_decl_at(const Parser* parser, uint32_t index) {
    if (index >= parser->decl_count) return NULL;
    return parser->decls[index];
}
