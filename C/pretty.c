#include "pretty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 辅助函数：计算字符串长度 */
static size_t term_str_len(const Term* term) {
    if (term == NULL) return 4;  /* "NULL" */
    
    switch (term->kind) {
        case TERM_VAR:
            if (term->var.name) {
                return snprintf(NULL, 0, "@%u(%s)", term->var.index, term->var.name);
            }
            return snprintf(NULL, 0, "@%u", term->var.index);
        
        case TERM_GLOBAL:
            return strlen(term->global);
        
        case TERM_TYPE:
            return 4;  /* "Type" */
        
        case TERM_PI:
            return snprintf(NULL, 0, "(%s: %s) -> %s",
                           term->pi.name ? term->pi.name : "_",
                           "",
                           "") + term_str_len(term->pi.domain) + term_str_len(term->pi.codomain);
        
        case TERM_LAM:
            return snprintf(NULL, 0, "\\%s: %s => %s",
                           term->lam.name ? term->lam.name : "_",
                           "",
                           "") + term_str_len(term->lam.param_type) + term_str_len(term->lam.body);
        
        case TERM_APP:
            return 1 + term_str_len(term->app.func) + 1 + term_str_len(term->app.arg) + 1;
    }
    
    return 0;
}

/* 辅助函数：递归构建字符串 */
static void term_to_str(const Term* term, char* buf, size_t* pos) {
    if (term == NULL) {
        *pos += sprintf(buf + *pos, "NULL");
        return;
    }
    
    switch (term->kind) {
        case TERM_VAR:
            if (term->var.name) {
                *pos += sprintf(buf + *pos, "@%u(%s)", term->var.index, term->var.name);
            } else {
                *pos += sprintf(buf + *pos, "@%u", term->var.index);
            }
            break;
        
        case TERM_GLOBAL:
            *pos += sprintf(buf + *pos, "%s", term->global);
            break;
        
        case TERM_TYPE:
            *pos += sprintf(buf + *pos, "Type");
            break;
        
        case TERM_PI:
            *pos += sprintf(buf + *pos, "(%s: ", term->pi.name ? term->pi.name : "_");
            term_to_str(term->pi.domain, buf, pos);
            *pos += sprintf(buf + *pos, ") -> ");
            term_to_str(term->pi.codomain, buf, pos);
            break;
        
        case TERM_LAM:
            *pos += sprintf(buf + *pos, "\\%s: ", term->lam.name ? term->lam.name : "_");
            term_to_str(term->lam.param_type, buf, pos);
            *pos += sprintf(buf + *pos, " => ");
            term_to_str(term->lam.body, buf, pos);
            break;
        
        case TERM_APP:
            *pos += sprintf(buf + *pos, "(");
            term_to_str(term->app.func, buf, pos);
            *pos += sprintf(buf + *pos, " ");
            term_to_str(term->app.arg, buf, pos);
            *pos += sprintf(buf + *pos, ")");
            break;
    }
}

char* show_term(const Term* term) {
    if (term == NULL) {
        return strdup("NULL");
    }
    
    /* 估算缓冲区大小 */
    size_t len = term_str_len(term) + 1;
    char* buf = (char*)malloc(len + 64);  /* 额外空间 */
    if (buf == NULL) return NULL;
    
    size_t pos = 0;
    term_to_str(term, buf, &pos);
    buf[pos] = '\0';
    
    return buf;
}

void print_term(const Term* term) {
    char* str = show_term(term);
    if (str) {
        printf("%s", str);
        free(str);
    }
}
