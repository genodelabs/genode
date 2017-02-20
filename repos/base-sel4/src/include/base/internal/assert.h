/*
 * \brief  Low-level assert macro
 * \author Norman Feske
 * \date   2015-05-06
 *
 * The output of the assert macro is directed to the platform's low-level
 * debugging facility.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__ASSERT_H_
#define _INCLUDE__BASE__INTERNAL__ASSERT_H_

/* Genode includes */
#include <base/snprintf.h>

/* base-internal includes */
#include <base/internal/kernel_debugger.h>

#define ASSERT(e) \
	do { if (!(e)) { \
		char line_buf[32]; \
		Genode::snprintf(line_buf, sizeof(line_buf), "%d", __LINE__); \
		kernel_debugger_outstring("Assertion failed: " #e "\n"); \
		kernel_debugger_outstring(__FILE__ ":"); \
		kernel_debugger_outstring(line_buf); \
		kernel_debugger_panic("\n"); \
		} \
	} while(0)
#else

#endif /* _INCLUDE__BASE__INTERNAL__ASSERT_H_ */
