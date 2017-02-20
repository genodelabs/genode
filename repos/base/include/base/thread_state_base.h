/*
 * \brief  Thread state base class
 * \author Norman Feske
 * \date   2007-07-30
 *
 * This file contains the generic part of the thread state.
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_BASE_H_
#define _INCLUDE__BASE__THREAD_STATE_BASE_H_

#include <cpu/cpu_state.h>

namespace Genode { struct Thread_state_base; }

struct Genode::Thread_state_base : Cpu_state
{
	bool unresolved_page_fault = false;
	bool exception = false;

	Thread_state_base() { };
	Thread_state_base(Cpu_state &c) : Cpu_state(c) { };
};

#endif /* _INCLUDE__BASE__THREAD_STATE_BASE_H_ */
