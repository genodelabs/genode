/*
 * \brief  Thread state
 * \author Stefan Kalkowski
 * \date   2010-01-20
 *
 * This file contains the Fiasco.OC specific part of the thread state.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FOC__THREAD_STATE_H_
#define _INCLUDE__FOC__THREAD_STATE_H_

#include <base/capability.h>
#include <base/thread_state.h>
#include <foc/syscall.h>

namespace Genode { struct Foc_thread_state; }


struct Genode::Foc_thread_state : Thread_state
{
	Foc::l4_cap_idx_t  kcap { Foc::L4_INVALID_CAP }; /* thread's gate cap in its PD */
	uint32_t           id   { };                     /* ID of gate capability */
	addr_t             utcb { };                     /* thread's UTCB in its PD */
};

#endif /* _INCLUDE__FOC__THREAD_STATE_H_ */
