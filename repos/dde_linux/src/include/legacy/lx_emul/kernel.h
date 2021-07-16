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

/*********************
 ** linux/kconfig.h **
 *********************/

#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val
#define ____is_defined(arg1_or_junk) __take_second_arg(arg1_or_junk 1, 0)
#define ___is_defined(val) ____is_defined(__ARG_PLACEHOLDER_##val)
#define __is_defined(x)    ___is_defined(x)
#define IS_BUILTIN(option) __is_defined(option)
#define IS_ENABLED(option) IS_BUILTIN(option)
#define IS_REACHABLE(option) IS_BUILTIN(option)

/********************
 ** linux/kernel.h **
 ********************/

/*
 * Log tags
 */
#define KERN_ALERT   "ALERT: "
#define KERN_CRIT    "CRTITCAL: "
#define KERN_DEBUG   "DEBUG: "
#define KERN_EMERG   "EMERG: "
#define KERN_ERR     "ERROR: "
#define KERN_INFO    "INFO: "
#define KERN_NOTICE  "NOTICE: "
#define KERN_WARNING "WARNING: "
#define KERN_WARN   "WARNING: "

struct va_format
{
	const char *fmt;
	va_list    *va;
};

static inline int _printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lx_vprintf(fmt, args);
	va_end(args);
	return 0;
}

/*
 * Debug macros
 */
#if DEBUG_LINUX_PRINTK
#define printk  _printk
#define vprintk lx_vprintf
#else
#define printk(...)
#define vprintk(...)
#endif

extern void lx_sleep_forever() __attribute__((noreturn));


static inline __printf(1, 2) void panic(const char *fmt, ...) __noreturn;
static inline void panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lx_vprintf(fmt, args);
	va_end(args);
	lx_printf("\npanic()\n");
	lx_sleep_forever();
}

/*
 * Bits and types
 */

/* needed by linux/list.h */
#define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

/* normally provided by linux/stddef.h, needed by linux/list.h */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define max_t(type, x, y) ({                    \
        type __max1 = (x);                      \
        type __max2 = (y);                      \
        __max1 > __max2 ? __max1: __max2; })

#define max(x, y) ({                      \
        typeof(x) _max1 = (x);                  \
        typeof(y) _max2 = (y);                  \
        _max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({ \
        type __min1 = (x); \
        type __min2 = (y); \
        __min1 < __min2 ? __min1: __min2; })

#define min(x, y) ({                      \
        typeof(x) _min1 = (x);                  \
        typeof(y) _min2 = (y);                  \
        _min1 > _min2 ? _min2 : _min1; })

#define abs(x) ( { \
                  typeof (x) _x = (x); \
                  _x < 0 ? -_x : _x;  })

#define lower_32_bits(n) ((u32)(n))
#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

#define roundup(x, y) (                           \
{                                                 \
        const typeof(y) __y = y;                        \
        (((x) + (__y - 1)) / __y) * __y;                \
})

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

#define clamp_val(val, min, max) ({             \
        typeof(val) __val = (val);              \
        typeof(val) __min = (min);              \
        typeof(val) __max = (max);              \
        __val = __val < __min ? __min: __val;   \
        __val > __max ? __max: __val; })

#define clamp_t(type, val, min, max) ({       \
        type __val = (val);                   \
        type __min = (min);                   \
        type __max = (max);                   \
        __val = __val < __min ? __min: __val; \
        __val > __max ? __max: __val; })

#define DIV_ROUND_CLOSEST(x, divisor)(      \
{                                           \
        typeof(x) __x = x;                        \
        typeof(divisor) __d = divisor;            \
        (((typeof(x))-1) > 0 ||                   \
        ((typeof(divisor))-1) > 0 || (__x) > 0) ? \
        (((__x) + ((__d) / 2)) / (__d)) :         \
        (((__x) - ((__d) / 2)) / (__d));          \
})

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define ALIGN(x, a)                   __ALIGN_KERNEL((x), (a))
#define __ALIGN_KERNEL(x, a)          __ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)  (((x) + (mask)) & ~(mask))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define BUILD_BUG_ON(condition)

#define _RET_IP_  (unsigned long)__builtin_return_address(0)

void might_sleep();
#define might_sleep_if(cond) do { if (cond) might_sleep(); } while (0)

#define INT_MAX   ((int)(~0U>>1))
#define UINT_MAX  (~0U)
#define INT_MIN   (-INT_MAX - 1)
#define USHRT_MAX ((u16)(~0U))
#define LONG_MAX  ((long)(~0UL>>1))
#define SHRT_MAX  ((s16)(USHRT_MAX>>1))
#define SHRT_MIN  ((s16)(-SHRT_MAX - 1))
#define ULONG_MAX (~0UL)

#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#define max3(x, y, z) max((typeof(x))max(x, y), z)
