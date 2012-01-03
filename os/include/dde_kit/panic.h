/*
 * \brief  Debugging support
 * \author Christian Helmuth
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__PANIC_H_
#define _INCLUDE__DDE_KIT__PANIC_H_

/**
 * Print message and block
 *
 * \param fmt  message format string
 */
void dde_kit_panic(const char *fmt, ...);

/**
 * Print debug message and block
 *
 * \param fmt  message format string
 *
 * Logs the debug message with appended newline.
 */
void dde_kit_debug(const char *fmt, ...);

#endif /* _INCLUDE__DDE_KIT__PANIC_H_ */
