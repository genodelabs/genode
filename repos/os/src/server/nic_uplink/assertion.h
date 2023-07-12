/*
 * \brief  Assertion macros
 * \author Martin Stein
 * \date   2023-07-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ASSERTION_H_
#define _ASSERTION_H_

/* base includes */
#include <base/log.h>
#include <base/sleep.h>

#define ASSERT(condition) \
	do { \
		if (!(condition)) { \
			Genode::error(__FILE__, ":", __LINE__, ": ", " assertion \"", #condition, "\" failed "); \
			Genode::sleep_forever(); \
		} \
	} while (false)

#define ASSERT_NEVER_REACHED \
	do { \
		Genode::error(__FILE__, ":", __LINE__, ": ", " should have never been reached"); \
		Genode::sleep_forever(); \
	} while (false)

#endif /* _ASSERTION_H_ */
