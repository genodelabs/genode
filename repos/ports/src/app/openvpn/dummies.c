/**
 * \brief  Dummy functions
 * \author Josef Soentgen
 * \date   2014-05-19
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <stdio.h>

typedef long DUMMY;

enum {
	SHOW_DUMMY = 0,
};

#define DUMMY(retval, name) \
DUMMY name(void) { \
	if (SHOW_DUMMY) \
		fprintf(stderr, #name " called (from %p) not implemented", __builtin_return_address(0)); \
	return retval; \
}

DUMMY(-1, mlockall)
DUMMY(-1, if_indextoname)
DUMMY(-1, if_nametoindex)
DUMMY(-1, sendmsg)
