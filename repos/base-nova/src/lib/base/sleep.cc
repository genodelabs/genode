/*
 * \brief  Lay back and relax
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/lock.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

void Genode::sleep_forever()
{
	using namespace Nova;

	Thread *myself = Thread::myself();
	addr_t sem = myself ? myself->native_thread().exc_pt_sel + SM_SEL_EC : SM_SEL_EC;

	while (1) {
		if (Nova::sm_ctrl(sem, SEMAPHORE_DOWNZERO))
			nova_die();
	}
}
