/*
 * \brief  'PDBG()' implementation for use in '.c' files
 * \author Christian Prochaska
 * \date   2013-07-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_PDBG_H_
#define _LIBC_PDBG_H_

extern void genode_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * Suppress debug messages in release version
 */
#ifdef GENODE_RELEASE
#define DO_PDBG 0
#else
#define DO_PDBG 1
#endif /* GENODE_RELEASE */

#define ESC_DBG  "\033[33m"
#define ESC_END  "\033[0m"

/**
 * Print debug message with function name
 */
#define PDBG(fmt, ...) \
	if (DO_PDBG) {\
		genode_printf("%s: " ESC_DBG fmt ESC_END "\n", \
                      __PRETTY_FUNCTION__, ##__VA_ARGS__ ); }

#endif /* _LIBC_DEBUG_H_ */
