/*
 * \brief  Assertion macro
 * \author Christian Helmuth
 * \date   2008-11-04
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__ASSERT_H_
#define _INCLUDE__DDE_KIT__ASSERT_H_

#include <dde_kit/printf.h>
#include <dde_kit/panic.h>

/**
 * Assert a condition
 *
 * \param expr  expression that must be true
 *
 * If the expression is not true, dde_kit_panic() is called.
 */
#define dde_kit_assert(expr)                                       \
	do {                                                           \
		if (!(expr)) {                                             \
			dde_kit_print("Assertion failed: "#expr"\n");          \
			dde_kit_printf("  File: %s:%d\n", __FILE__, __LINE__); \
			dde_kit_printf("  Function: %s()\n", __FUNCTION__);    \
			dde_kit_panic("Assertion failed.");                    \
		}                                                          \
	} while (0);

#endif /* _INCLUDE__DDE_KIT__ASSERT_H_ */
