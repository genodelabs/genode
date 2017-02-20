/*
 * \brief  Thread state
 * \author Norman Feske
 * \date   2007-07-30
 *
 * This file provides a generic implementation of the 'Thread state' class.
 * Base platforms can provide their own version of this file.
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
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
	Thread_state() {}
	Thread_state(Thread_state_base &base) : Thread_state_base(base) {}
};

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
