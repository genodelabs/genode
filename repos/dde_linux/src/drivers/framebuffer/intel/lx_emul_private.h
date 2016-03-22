/*
 * \brief  Local definitions of the Linux kernel API implementation
 * \author Norman Feske
 * \date   2015-08-24
 */

#ifndef _LX_EMUL_PRIVATE_H_
#define _LX_EMUL_PRIVATE_H_

/* Linux kernel API */
#include <stdarg.h>
#include <lx_emul/printf.h>

#if 0
#define TRACE \
	do { \
		lx_printf("%s not implemented\n", __func__); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
	do { \
		lx_printf("%s not implemented\n", __func__); \
		BUG(); \
	} while (0)

#define ASSERT(x) \
	do { \
		if (!(x)) { \
			lx_printf("%s:%u assertion failed\n", __func__, __LINE__); \
			BUG(); \
		} \
	} while (0)

#endif /* _LX_EMUL_PRIVATE_H_ */
