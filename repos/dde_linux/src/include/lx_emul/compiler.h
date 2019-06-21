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

/**********************
 ** linux/compiler.h **
 **********************/

#define likely
#define unlikely

#define __user
#define __iomem
#define __rcu
#define __percpu
#define __must_check
#define __force
#define __read_mostly

#define __releases(x) /* needed by usb/core/devio.c */
#define __acquires(x) /* needed by usb/core/devio.c */
#define __force
#define __maybe_unused
#define __bitwise

# define __acquire(x) (void)0
# define __release(x) (void)0

#define __must_check

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#define __attribute_const__
#undef  __always_inline
#define __always_inline inline
#undef __unused

#define noinline     __attribute__((noinline))

#define __printf(a, b) __attribute__((format(printf, a, b)))

#define __always_unused
#define __read_mostly

#define __noreturn    __attribute__((noreturn))

#define WRITE_ONCE(x, val) \
({                                                      \
        barrier(); \
        union { typeof(x) v; char c[1]; } u = \
              { .v = (typeof(x)) (val) }; \
        __builtin_memcpy((void *)&(x), (const void *)u.c, sizeof(x)); \
        barrier(); \
})

/* XXX alpha, powerpc, blackfin needs proper implementation */
#define smp_read_barrier_depends() do { } while (0)
#define smp_store_mb(var, value)  do { WRITE_ONCE(var, value); barrier(); } while (0)

#ifndef __compiletime_object_size
# define __compiletime_object_size(obj) -1
#endif

/**************************
 ** linux/compiler-gcc.h **
 **************************/

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define __weak __attribute__((weak))

#define __aligned(x)  __attribute__((aligned(x)))

#define uninitialized_var(x) x = x

#define unreachable() \
	do { __builtin_unreachable(); } while (0)
