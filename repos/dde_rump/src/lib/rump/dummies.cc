/**
 * \brief  Dummy functions
 * \author Sebastian Sumpf
 * \date   2013-12-06
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

extern "C" {

enum {
	SHOW_DUMMY = 1,
};

#define DUMMY(retval, name) \
	int name(void) { \
	if (SHOW_DUMMY) \
		Genode::warning(#name " called (from ", __builtin_return_address(0), ") " \
		                "not implemented"); \
	return retval; \
}

#define DUMMY_RET(retval, name) \
	int name(void) { \
	return retval; \
}

DUMMY(-1, rumpuser_anonmmap)
DUMMY(-1, rumpuser_close)
DUMMY(-1, rumpuser_daemonize_begin)
DUMMY(-1, rumpuser_daemonize_done)
DUMMY(-1, rumpuser_iovread)
DUMMY(-1, rumpuser_iovwrite)
DUMMY(-1, rumpuser_kill)
DUMMY(-1, rumpuser_sp_anonmmap)
DUMMY(-1, rumpuser_sp_copyin)
DUMMY(-1, rumpuser_sp_copyinstr)
DUMMY(-1, rumpuser_sp_copyout)
DUMMY(-1, rumpuser_sp_copyoutstr)
DUMMY(-1, rumpuser_sp_fini)
DUMMY(-1, rumpuser_sp_init)
DUMMY(-1, rumpuser_sp_raise)
DUMMY(-1, rumpuser_thread_join)
DUMMY(-1, rumpuser_unmap)
} /* extern "C" */
