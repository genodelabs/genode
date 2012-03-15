/*
 * \brief  Thread state
 * \author Stefan Kalkowski
 * \date   2010-01-20
 *
 * This file contains the Fiasco.OC specific part of the thread state.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

#include <base/capability.h>
#include <base/lock.h>
#include <cpu/cpu_state.h>

namespace Genode {

	struct Thread_state : public Cpu_state
	{
		Native_thread         kcap;         /* thread's gate cap in its pd */
		int                   id;           /* id of gate capability */
		Native_utcb           utcb;         /* thread's utcb in its pd */
		unsigned              exceptions;   /* counts exceptions raised by the thread */
		bool                  paused;       /* indicates whether thread is stopped */
		bool                  in_exception; /* true if thread is in exception */
		Lock                  lock;

		/**
		 * Constructor
		 */
		Thread_state()
		: kcap(Fiasco::L4_INVALID_CAP), id(0), utcb(0), exceptions(0),
		  paused(false), in_exception(false) { }
	};
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
