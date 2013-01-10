/*
 * \brief  Codezero-specific implementation of cmpxchg
 * \author Norman Feske
 * \date   2009-10-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cpu/atomic.h>
#include <base/printf.h>
#include <base/lock.h>

/* Codezero includes */
#include <codezero/syscalls.h>


static bool mutex_initialized;
static Codezero::l4_mutex mutex;

int Genode::cmpxchg(volatile int *dest, int cmp_val, int new_val)
{
	if (!mutex_initialized) {
		Codezero::l4_mutex_init(&mutex);
		mutex_initialized = true;
	}

	int ret = Codezero::l4_mutex_lock(&mutex);
	if (ret < 0)
		mutex_initialized = false;

	bool result = false;
	if (*dest == cmp_val) {
		*dest = new_val;
		result = true;
	}

	ret = Codezero::l4_mutex_unlock(&mutex);
	if (ret < 0)
		mutex_initialized = false;

	return result;
}
