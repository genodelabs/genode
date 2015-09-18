/*
 * \brief  Local definitions of the Linux kernel API implementation
 * \author Norman Feske
 * \date   2015-08-24
 */

#ifndef _LX_EMUL_PRIVATE_H_
#define _LX_EMUL_PRIVATE_H_

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

/* Linux kernel API */
#include <lx_emul.h>

#if 0
#define TRACE \
	do { \
		PLOG("%s not implemented", __func__); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
	do { \
		PWRN("%s not implemented", __func__); \
		Genode::sleep_forever(); \
	} while (0)

#define ASSERT(x) \
	do { \
		if (!(x)) { \
			PWRN("%s:%u assertion failed", __func__, __LINE__); \
			Genode::sleep_forever(); \
		} \
	} while (0)

#endif /* _LX_EMUL_PRIVATE_H_ */
