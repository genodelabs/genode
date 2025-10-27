/*
 * \brief  Assertion macro
 * \author Stefan Kalkowski
 * \date   2025-10-27
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__ASSERT_H_
#define _SRC__BOOTSTRAP__ASSERT_H_

/* Genode includes */
#include <base/log.h>

#define ASSERT(expression) \
	do { \
		if (!(expression)) { \
			Genode::error("Assertion failed: "#expression""); \
			Genode::error("  File: ", __FILE__, ":", __LINE__); \
			Genode::error("  Function: ", __PRETTY_FUNCTION__); \
			while (true) ; \
		} \
	} while (false) ;

#endif /* _SRC__BOOTSTRAP__ASSERT_H_ */
