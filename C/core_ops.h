#ifndef CORE_OPS_H
#define CORE_OPS_H

#include "term.h"

/* ============================================================
 * De Bruijn 操作
 * ============================================================ */

/* shift(term, amount, cutoff)
 * 将 term 中所有 index >= cutoff 的变量增加 amount
 */
Term* term_shift(const Term* term, uint32_t amount, uint32_t cutoff);

/* subst(term, index, replacement)
 * 将 term 中 index 位置的变量替换为 replacement
 */
Term* term_subst(const Term* term, uint32_t index, const Term* replacement);

/* instantiate(body, arg)
 * 将 body 中 index 0 的变量替换为 arg
 * 等价于 term_subst(body, 0, arg)
 */
Term* term_instantiate(const Term* body, const Term* arg);

/* instantiate_many(body, args, count)
 * 批量实例化：从右到左依次对 body 做多次 instantiate
 * instantiate_many(B, [a1, a2, a3]) = instantiate(instantiate(instantiate(B, a3), a2), a1)
 */
Term* term_instantiate_many(const Term* body, const Term** args, size_t count);

/* instantiate_env(term, env, env_size, depth)
 * 环境实例化：将 term 中从 depth 开始的 de Bruijn 变量批量替换为 env 中的值
 */
Term* term_instantiate_env(const Term* term, const Term** env, size_t env_size, uint32_t depth);

#endif /* CORE_OPS_H */
