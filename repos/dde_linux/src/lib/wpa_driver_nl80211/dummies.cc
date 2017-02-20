/*
 * \brief  Dummy functions
 * \author Josef Soentgen
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>

extern "C" {
	typedef long DUMMY;

enum {
	SHOW_DUMMY = 0,
	SHOW_SKIP  = 0,
	SHOW_RET   = 0,
};

#define DUMMY(retval, name) \
	DUMMY name(void) { \
		if (SHOW_DUMMY) \
			Genode::log(__func__, ": " #name " called " \
			            "(from ", __builtin_return_address(0), ") " \
			            "not implemented"); \
		return retval; \
	}

#define DUMMY_SKIP(retval, name) \
	DUMMY name(void) { \
		if (SHOW_SKIP) \
			Genode::log(__func__, ": " #name " called " \
			            "(from ", __builtin_return_address(0), ") " \
			            "skipped"); \
		return retval; \
	}

#define DUMMY_RET(retval, name) \
	DUMMY name(void) { \
		if (SHOW_RET) \
			Genode::log(__func__, ": " #name " called " \
			            "(from ", __builtin_return_address(0), ") " \
			            "return ", retval); \
		return retval; \
	}

DUMMY(0, getprotobyname)
DUMMY(0, getprotobynumber)
DUMMY(0, nl_addr_get_binary_addr)
DUMMY(0, nl_addr_get_len)
DUMMY(0, nl_hash_any)
DUMMY(0, rint)

} /* extern "C" */
