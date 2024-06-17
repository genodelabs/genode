/*
 * \brief  Thread state
 * \author Norman Feske
 * \date   2007-07-30
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

#include <cpu/cpu_state.h>

namespace Genode { struct Thread_state; }


struct Genode::Thread_state
{
	enum class State { VALID, UNAVAILABLE, PAGE_FAULT, EXCEPTION } state;

	Cpu_state cpu;
};

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
