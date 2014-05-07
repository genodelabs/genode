/*
 * \brief  Formatted output
 * \author Christian Helmuth
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__PRINTF_H_
#define _INCLUDE__DDE_KIT__PRINTF_H_

#include <stdarg.h>

/**
 * Print message
 *
 * \param msg  message string
 */
void dde_kit_print(const char *msg);

/**
 * Print formatted message (varargs variant)
 *
 * \param fmt  format string
 */
void dde_kit_printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

/**
 * Print formatted message (va_list variant)
 *
 * \param fmt  format string
 * \param va   variable argument list
 */
void dde_kit_vprintf(const char *fmt, va_list va) __attribute__ ((format (printf, 1, 0)));

/**
 * Log current function and message
 *
 * \param doit  if false logging is suppressed
 * \param msg   format string plus arguments
 */
#define dde_kit_log(doit, msg...)               \
	do {                                        \
		if (doit) {                             \
			dde_kit_printf("%s(): ", __func__); \
			dde_kit_printf(msg);                \
			dde_kit_printf("\n");               \
		}                                       \
	} while(0);

#endif /* _INCLUDE__DDE_KIT__PRINTF_H_ */
