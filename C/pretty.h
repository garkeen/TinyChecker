#ifndef PRETTY_H
#define PRETTY_H

#include "term.h"

/* 将 Term 转换为可读字符串 */
/* 返回的字符串需要调用者释放 */
char* show_term(const Term* term);

/* 将 Term 打印到标准输出 */
void print_term(const Term* term);

#endif /* PRETTY_H */
