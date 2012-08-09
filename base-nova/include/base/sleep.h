/*
 * \brief  Lay back and relax
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2010-02-01
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SLEEP_H_
#define _INCLUDE__BASE__SLEEP_H_

/* Genode includes */
#include <base/thread.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

namespace Genode {

	__attribute__((noreturn)) inline void sleep_forever()
	{
		using namespace Nova;

		Thread_base *myself = Thread_base::myself();
		addr_t sem = myself ? myself->tid().exc_pt_sel + SM_SEL_EC : SM_SEL_EC;

		while (1) {
			if (Nova::sm_ctrl(sem, SEMAPHORE_DOWNZERO))
				nova_die();
		}
	}
}

#endif /* _INCLUDE__BASE__SLEEP_H_ */
