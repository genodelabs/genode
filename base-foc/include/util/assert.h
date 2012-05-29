/*
 * \brief  Assertion macros for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2012-05-25
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ASSERT_H_
#define _INCLUDE__UTIL__ASSERT_H_

#include <base/printf.h>

namespace Fiasco {
#include <l4/sys/kdebug.h>
}

#if 1
#define ASSERT(e, s) \
	do { if (!(e)) {								   \
		Fiasco::outstring(ESC_ERR s ESC_END "\n");	   \
		Fiasco::outstring(__FILE__ ":");			   \
		Fiasco::outhex32((int)__LINE__);			   \
		Fiasco::outstring("\n");					   \
		enter_kdebug("ASSERT");				   \
		}											   \
	} while(0)
#else
#define ASSERT(e, s)  do { } while (0)
#endif

#endif /* _INCLUDE__UTIL__ASSERT_H_ */
