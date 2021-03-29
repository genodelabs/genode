/*
 * \brief  Dummy implementations of symbols needed by VirtualBox
 * \author Norman Feske
 * \date   2013-08-22
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _STUB_MACROS_H_
#define _STUB_MACROS_H_

#include <base/log.h>
#include <base/sleep.h>

#define TRACE(retval) \
	{ \
		if (debug) \
			Genode::log(__PRETTY_FUNCTION__, " called (", __FILE__, ") - eip=", \
			            __builtin_return_address(0)); \
		return retval; \
	}

#define STOP \
	{ \
		Genode::error(__PRETTY_FUNCTION__, " called (", __FILE__, ":", __LINE__, "), " \
		              "not implemented, eip=", \
		              __builtin_return_address(0)); \
		/* noreturn function sparing the need for a return value */ \
		Genode::sleep_forever(); \
	}

#endif /* _STUB_MACROS_H_ */
