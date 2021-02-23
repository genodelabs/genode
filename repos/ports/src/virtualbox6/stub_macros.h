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
		while (1); \
		\
		class Not_implemented { }; \
		throw Not_implemented(); /* sparing the need for a return value */ \
	}

#define DUMMY_STATIC(X) \
	{ \
		static X dummy; \
		Genode::error("static ", __PRETTY_FUNCTION__, " called (", __FILE__, "), " \
		              "not implemented, eip=", \
		              __builtin_return_address(0)); \
		while (1) \
			asm volatile ("ud2a"); \
		\
		return dummy; \
	}

#endif /* _STUB_MACROS_H_ */
