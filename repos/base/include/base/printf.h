/*
 * \brief  Interface of the printf back end
 * \author Norman Feske
 * \date   2006-04-08
 */

/*
 * Copyright (C) 2006-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PRINTF_H_
#define _INCLUDE__BASE__PRINTF_H_

#include <stdarg.h>

namespace Genode {

	/**
	 * Output format string to LOG session
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

/**
 * Suppress debug messages in release version
 */
#ifdef GENODE_RELEASE
#define DO_PWRN false
#else
#define DO_PWRN true
#endif /* GENODE_RELEASE */

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
	do { \
		if (DO_PWRN) \
			Genode::printf(ESC_WRN fmt ESC_END "\n", ##__VA_ARGS__ ); \
	} while (0)

/**
 * Print error message
 */
#define PERR(fmt, ...) \
	Genode::printf(ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__ )

#endif /* _INCLUDE__BASE__PRINTF_H_ */
