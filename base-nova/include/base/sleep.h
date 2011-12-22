/*
 * \brief  Lay back and relax
 * \author Norman Feske
 * \date   2010-02-01
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SLEEP_H_
#define _INCLUDE__BASE__SLEEP_H_

/* Genode includes */
#include <base/cap_sel_alloc.h>
#include <base/thread.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode {

	__attribute__((noreturn)) inline void sleep_forever()
	{
		int sleep_sm_sel = cap_selector_allocator()->alloc();
		Nova::create_sm(sleep_sm_sel, Cap_selector_allocator::pd_sel(), 0);
		while (1) Nova::sm_ctrl(sleep_sm_sel, Nova::SEMAPHORE_DOWN);
	}
}

#endif /* _INCLUDE__BASE__SLEEP_H_ */
