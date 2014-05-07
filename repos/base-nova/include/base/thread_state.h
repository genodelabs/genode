/*
 * \brief  Thread state
 * \author Alexander Boettcher
 * \date   2012-08-09
 *
 * This file contains the NOVA specific thread state.
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

#include <base/thread_state_base.h>

namespace Genode {

	struct Thread_state : Thread_state_base
	{
		bool is_vcpu;
		addr_t sel_exc_base;

		Thread_state() : is_vcpu(false), sel_exc_base(~0UL) { }

		Thread_state(bool is_vcpu, addr_t sel_exc_base)
		: is_vcpu(is_vcpu), sel_exc_base(sel_exc_base) { }
	};
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
