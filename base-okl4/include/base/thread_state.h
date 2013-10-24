/*
 * \brief  Thread state
 * \author Stefan Kalkowski
 * \date   2007-07-30
 *
 * This file contains the OKL4 specific part of the thread state.
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

namespace Okl4 { extern "C" {
#include <l4/types.h>
} }

#include <base/thread_state_base.h>

namespace Genode {

	struct Thread_state : Thread_state_base
	{
		Okl4::L4_ThreadId_t tid;     /* OKL4 specific thread id */
	};
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
