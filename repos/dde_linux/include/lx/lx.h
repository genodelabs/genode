#ifndef _INCLUDE__LX__LX_H_
#define _INCLUDE__LX__LX_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

typedef genode_int8_t   int8_t;
typedef genode_int16_t  int16_t;
typedef genode_int32_t  int32_t;
typedef genode_uint32_t uint32_t;
typedef genode_int64_t  int64_t;
typedef genode_uint8_t  uint8_t;
typedef genode_uint16_t uint16_t;
typedef genode_uint64_t uint64_t;
typedef __SIZE_TYPE__   size_t;

void lx_printf(char const *, ...) __attribute__((format(printf, 1, 2)));
void lx_vprintf(char const *, va_list);

#define lx_log(doit, msg...)         \
	do {                               \
		if (doit) {                      \
			lx_printf("%s(): ", __func__); \
			lx_printf(msg);                \
			lx_printf("\n");               \
		}                                \
	} while(0)


/**********************
 ** linux/compiler.h **
 **********************/

#define __printf(a, b) __attribute__((format(printf, a, b)))


/**************************
 ** linux/compiler-gcc.h **
 **************************/

#define __noreturn    __attribute__((noreturn))

/***************
 ** asm/bug.h **
 ***************/

#define WARN_ON(condition) ({ \
	int ret = !!(condition); \
	if (ret) lx_printf("[%s] WARN_ON(" #condition ") ", __func__); \
	ret; })

#define WARN(condition, fmt, arg...) ({ \
	int ret = !!(condition); \
	if (ret) lx_printf("[%s] *WARN* " fmt , __func__ , ##arg); \
	ret; })

#define BUG() do { \
	lx_printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1); \
} while (0)

#define WARN_ON_ONCE WARN_ON
#define WARN_ONCE    WARN

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)


/********************
 ** linux/kernel.h **
 ********************/

static inline __printf(1, 2) void panic(const char *fmt, ...) __noreturn;
static inline void panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	lx_vprintf(fmt, args);
	va_end(args);
	lx_printf("panic()");
	while (1) ;
}

#endif /* _INCLUDE__LX__LX_H_ */
