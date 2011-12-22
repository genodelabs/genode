/*
 * \brief  Thread state
 * \author Stefan Kalkowski
 * \date   2010-01-20
 *
 * This file contains the Fiasco.OC specific part of the thread state.
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
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
		Native_capability     cap;          /* capability selector with thread cap */
		unsigned              exceptions;   /* counts exceptions raised by the thread */
		bool                  paused;       /* indicates whether thread is stopped */
		bool                  in_exception; /* true if thread is currently in exception */
		Lock                  lock;

		/**
		 * Constructor
		 */
		Thread_state() : cap(), exceptions(0), paused(false) { }
	};
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
