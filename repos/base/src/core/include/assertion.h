/*
 * \brief  Assertion macros
 * \author Norman Feske
 * \date   2016-01-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__ASSERTION_H_
#define _CORE__INCLUDE__ASSERTION_H_

/* Genode includes */
#include <base/log.h>

#define ASSERT_NEVER_CALLED \
	Genode::error("Unexpected call of '", __FUNCTION__, "' " \
	              "(", __FILE__, ":", __LINE__, ")"); \
	for (;;); throw 0UL;

#endif /* _CORE__INCLUDE__ASSERTION_H_ */
