/*
 * \brief  Interface of the printf backend
 * \author Norman Feske
 * \date   2006-04-08
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PRINTF_H_
#define _INCLUDE__BASE__PRINTF_H_

#include <stdarg.h>

namespace Genode {

	/**
	 * For your convenience...
	 */
	void  printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	void vprintf(const char *format, va_list) __attribute__((format(printf, 1, 0)));
}

#define ESC_LOG  "\033[33m"
#define ESC_DBG  "\033[33m"
#define ESC_INF  "\033[32m"
#define ESC_WRN  "\033[34m"
#define ESC_ERR  "\033[31m"
#define ESC_END  "\033[0m"

/**
 * Remove colored output from release version
 */
#ifdef GENODE_RELEASE
#undef ESC_LOG
#undef ESC_DBG
#undef ESC_INF
#undef ESC_WRN
#undef ESC_ERR
#undef ESC_END
#define ESC_LOG
#define ESC_DBG
#define ESC_INF
#define ESC_WRN
#define ESC_ERR
#define ESC_END
#endif /* GENODE_RELEASE */

/*
 * We're using heavy CPP wizardry here to prevent compiler errors after macro
 * expansion. Each macro works as follows:
 *
 * - Support one format string plus zero or more arguments.
 * - Put all static strings (including the format string) in the first argument
 *   of the call to printf() and let the compiler merge them.
 * - Append the function name (magic static string variable) as first argument.
 * - (Optionally) append the arguments to the macro with ", ##__VA_ARGS__". CPP
 *   only appends the comma and arguments if __VA__ARGS__ is not empty,
 *   otherwise nothing (not even the comma) is appended.
 */

/**
 * Suppress debug messages in release version
 */
#ifdef GENODE_RELEASE
#define DO_PDBG false
#else
#define DO_PDBG true
#endif /* GENODE_RELEASE */

/**
 * Print debug message with function name
 */
#define PDBG(fmt, ...) \
	if (DO_PDBG) \
		Genode::printf("%s: " ESC_DBG fmt ESC_END "\n", \
		               __PRETTY_FUNCTION__, ##__VA_ARGS__ )

/**
 * Print log message
 */
#define PLOG(fmt, ...) \
	Genode::printf(ESC_LOG fmt ESC_END "\n", ##__VA_ARGS__ )

/**
 * Print status-information message
 */
#define PINF(fmt, ...) \
	Genode::printf(ESC_INF fmt ESC_END "\n", ##__VA_ARGS__ )

/**
 * Print warning message
 */
#define PWRN(fmt, ...) \
	Genode::printf(ESC_WRN fmt ESC_END "\n", ##__VA_ARGS__ )

/**
 * Print error message
 */
#define PERR(fmt, ...) \
	Genode::printf(ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__ )

#endif /* _INCLUDE__BASE__PRINTF_H_ */
