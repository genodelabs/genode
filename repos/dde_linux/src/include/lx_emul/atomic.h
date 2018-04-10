/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************
 ** asm/cmpxchg.h **
 *******************/

#define cmpxchg(ptr, o, n) ({ \
		typeof(*ptr) prev = *ptr; \
		*ptr = (*ptr == o) ? n : *ptr; \
		prev;\
		})

#define xchg(ptr, x) ({ \
	typeof(*ptr) old = *ptr; \
	*ptr = x; \
	old; \
})

#define atomic_xchg(ptr, v)		(xchg(&(ptr)->counter, (v)))


/******************
 ** asm/atomic.h **
 ******************/

#include <asm-generic/atomic64.h>

#define ATOMIC_INIT(i) { (i) }

typedef struct atomic { int  counter; } atomic_t;
typedef struct        { long counter; } atomic_long_t;

static inline int  atomic_read(const atomic_t *p)          { return p->counter; }
static inline void atomic_set(atomic_t *p, int i)          { p->counter = i; }
static inline void atomic_sub(int i, atomic_t *p)          { p->counter -= i; }
static inline void atomic_add(int i, atomic_t *p)          { p->counter += i; }
static inline int  atomic_sub_return(int i, atomic_t *p)   { p->counter -= i; return p->counter; }
static inline int  atomic_add_return(int i, atomic_t *p)   { p->counter += i; return p->counter; }
static inline int  atomic_sub_and_test(int i, atomic_t *p) { return atomic_sub_return(i, p) == 0; }

static inline void atomic_dec(atomic_t *p)          { atomic_sub(1, p); }
static inline void atomic_inc(atomic_t *p)          { atomic_add(1, p); }
static inline int  atomic_dec_return(atomic_t *p)   { return atomic_sub_return(1, p); }
static inline int  atomic_inc_return(atomic_t *p)   { return atomic_add_return(1, p); }
static inline int  atomic_dec_and_test(atomic_t *p) { return atomic_sub_and_test(1, p); }
static inline int  atomic_inc_not_zero(atomic_t *p) { return p->counter ? atomic_inc_return(p) : 0; }

static inline void atomic_long_inc(atomic_long_t *p)                { p->counter += 1; }
static inline void atomic_long_sub(long i, atomic_long_t *p)        { p->counter -= i; }
static inline long atomic_long_add_return(long i, atomic_long_t *p) { p->counter += i; return p->counter; }
static inline long atomic_long_read(atomic_long_t *p)               { return p->counter; }

static inline int atomic_cmpxchg(atomic_t *v, int old, int n)
{
	return cmpxchg(&v->counter, old, n);
}

static inline int atomic_inc_not_zero_hint(atomic_t *v, int hint)
{
	int val, c = hint;

	/* sanity test, should be removed by compiler if hint is a constant */
	if (!hint)
		return atomic_inc_not_zero(v);

	do {
		val = atomic_cmpxchg(v, c, c + 1);
		if (val == c)
			return 1;
		c = val;
	} while (c);

	return 0;
}

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	int ret;
	unsigned long flags;
	(void)flags;

	ret = v->counter;
	if (ret != u)
		v->counter += a;

	return ret != u;
}

static inline int atomic_dec_if_positive(atomic_t *v)
{
	int c = atomic_read(v);

	if (c >= 0)
		atomic_dec(v);

	return c - 1;
}

#define smp_mb__before_atomic_dec()


/*******************************
 ** asm-generic/atomic-long.h **
 *******************************/

#define ATOMIC_LONG_INIT(i) ATOMIC_INIT(i)
