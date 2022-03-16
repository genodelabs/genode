/**
 * \brief  Shadow copy of asm/atomic64_32.h
 * \author Josef Soentgen
 * \date   2022-01-31
 *
 * This header contains all declarations but only a handful are
 * actually implemented.
 */

#ifndef _ASM__ATOMIC64_32_H_
#define _ASM__ATOMIC64_32_H_

#include <linux/types.h>

typedef struct {
    s64 __aligned(8) counter;
} atomic64_t;

#define ATOMIC64_INIT(val)	{ (val) }

s64  arch_atomic64_add(s64 i, atomic64_t *v);
s64  arch_atomic64_add_return(s64 i, atomic64_t *v);
int  arch_atomic64_add_unless(atomic64_t *v, s64 a, s64 u);
void arch_atomic64_and(s64 i, atomic64_t *v);
s64  arch_atomic64_cmpxchg(atomic64_t *v, s64 o, s64 n);
void arch_atomic64_dec(atomic64_t *v);
s64  arch_atomic64_dec_if_positive(atomic64_t *v);
s64  arch_atomic64_dec_return(atomic64_t *v);
s64  arch_atomic64_fetch_add(s64 i, atomic64_t *v);
s64  arch_atomic64_fetch_and(s64 i, atomic64_t *v);
s64  arch_atomic64_fetch_or(s64 i, atomic64_t *v);
s64  arch_atomic64_fetch_xor(s64 i, atomic64_t *v);
void arch_atomic64_inc(atomic64_t *v);
int  arch_atomic64_inc_not_zero(atomic64_t *v);
s64  arch_atomic64_inc_return(atomic64_t *v);
void arch_atomic64_or(s64 i, atomic64_t *v);
s64  arch_atomic64_read(const atomic64_t *v);
void arch_atomic64_set(atomic64_t *v, s64 i);
s64  arch_atomic64_sub(s64 i, atomic64_t *v);
s64  arch_atomic64_sub_return(s64 i, atomic64_t *v);
s64  arch_atomic64_xchg(atomic64_t *v, s64 n);
void arch_atomic64_xor(s64 i, atomic64_t *v);

#define arch_atomic64_cmpxchg arch_atomic64_cmpxchg
#define arch_atomic64_xchg arch_atomic64_xchg
#define arch_atomic64_add_return arch_atomic64_add_return
#define arch_atomic64_sub_return arch_atomic64_sub_return
#define arch_atomic64_inc_return arch_atomic64_inc_return
#define arch_atomic64_dec_return arch_atomic64_dec_return
#define arch_atomic64_inc arch_atomic64_inc
#define arch_atomic64_dec arch_atomic64_dec
#define arch_atomic64_add_unless arch_atomic64_add_unless
#define arch_atomic64_inc_not_zero arch_atomic64_inc_not_zero
#define arch_atomic64_dec_if_positive arch_atomic64_dec_if_positive
#define arch_atomic64_fetch_and arch_atomic64_fetch_and
#define arch_atomic64_fetch_or arch_atomic64_fetch_or
#define arch_atomic64_fetch_xor arch_atomic64_fetch_xor
#define arch_atomic64_fetch_add arch_atomic64_fetch_add
#define arch_atomic64_fetch_sub(i, v)   arch_atomic64_fetch_add(-(i), (v))

#endif /* _ASM__ATOMIC64_32_H_ */
