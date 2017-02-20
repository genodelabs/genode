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
#include <base/lock.h>
#include <base/thread_state.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode { struct Foc_thread_state; }


struct Genode::Foc_thread_state : Thread_state
{
	Fiasco::l4_cap_idx_t  kcap;         /* thread's gate cap in its pd */
	int                   id;           /* id of gate capability */
	addr_t                utcb;         /* thread's utcb in its pd */
	unsigned              exceptions;   /* counts exceptions raised by the thread */
	bool                  paused;       /* indicates whether thread is stopped */
	bool                  in_exception; /* true if thread is in exception */
	Lock                  lock;

	/**
	 * Constructor
	 */
	Foc_thread_state()
	: kcap(Fiasco::L4_INVALID_CAP), id(0), utcb(0), exceptions(0),
	  paused(false), in_exception(false) { }
};

#endif /* _INCLUDE__FOC__THREAD_STATE_H_ */
