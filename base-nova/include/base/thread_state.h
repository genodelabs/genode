/*
 * \brief  Thread state
 * \author Alexander Boettcher
 * \date   2012-08-09
 *
 * This file contains the NOVA specific thread state.
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

#include <cpu/cpu_state.h>

namespace Genode {

	struct Thread_state : public Cpu_state
	{
		bool transfer;
		bool is_vcpu;
		addr_t sel_exc_base;

		Thread_state(bool trans = false) : Cpu_state(), transfer(trans),
		                                   is_vcpu(false), sel_exc_base(~0UL) {}
	};
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
