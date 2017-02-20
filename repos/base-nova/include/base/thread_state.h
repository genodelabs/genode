/*
 * \brief  Thread state
 * \author Alexander Boettcher
 * \date   2012-08-09
 *
 * This file contains the NOVA specific thread state.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

#include <base/thread_state_base.h>

namespace Genode { struct Thread_state; }


struct Genode::Thread_state : Thread_state_base
{
	bool vcpu;
	addr_t sel_exc_base;
	bool global_thread;

	Thread_state() : vcpu(false), sel_exc_base(~0UL), global_thread(true) { }
};

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
