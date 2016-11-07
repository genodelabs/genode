/*
 * \brief  Assertion macro expected by seL4's kernel-interface headers
 * \author Norman Feske
 * \date   2014-10-15
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SEL4__ASSERT_H_
#define _INCLUDE__SEL4__ASSERT_H_

/* Genode includes */
#include <base/log.h>

#define seL4_Assert(v) do { \
	if (!(v)) { \
		Genode::error("assertion failed: ", #v); \
		for (;;); \
	} } while (0);

#endif /* _INCLUDE__SEL4__ASSERT_H_ */
