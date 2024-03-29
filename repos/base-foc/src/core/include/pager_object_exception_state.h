/*
 * \brief  Type used to store kernel-specific exception state
 * \author Norman Feske
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PAGER_OBJECT_EXCEPTION_STATE_H_
#define _CORE__INCLUDE__PAGER_OBJECT_EXCEPTION_STATE_H_

#include <base/mutex.h>
#include <foc/thread_state.h>

namespace Core { struct Pager_object_exception_state; }


struct Core::Pager_object_exception_state
{
	Mutex            mutex { };
	unsigned         exceptions;   /* counts exceptions raised by the thread */
	bool             paused;       /* indicates whether thread is stopped */
	bool             in_exception; /* true if thread is in exception */
	Foc_thread_state state;        /* accessible via native cpu thread RPC */
};

#endif /* _CORE__INCLUDE__PAGER_OBJECT_EXCEPTION_STATE_H_ */
