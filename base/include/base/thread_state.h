/*
 * \brief  Thread state
 * \author Norman Feske
 * \date   2007-07-30
 *
 * This file contains the generic part of the thread state.
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_STATE_H_
#define _INCLUDE__BASE__THREAD_STATE_H_

#include <cpu/cpu_state.h>

namespace Genode {

	struct Thread_state : public Cpu_state { };
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
