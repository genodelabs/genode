/*
 * \brief  Assertion macros for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2012-05-25
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__FOC_ASSERT_H_
#define _INCLUDE__BASE__INTERNAL__FOC_ASSERT_H_

/* Genode includes */
#include <base/log.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
}

#define ASSERT(e, s) \
	do { if (!(e)) { \
		Genode::raw("assertion failed: ", s, __FILE__, __LINE__); \
		enter_kdebug("ASSERT"); \
		} \
	} while(0)

#endif /* _INCLUDE__BASE__INTERNAL__FOC_ASSERT_H_ */
