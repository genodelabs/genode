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

	struct Thread_state : public Cpu_state { };
}

#endif /* _INCLUDE__BASE__THREAD_STATE_H_ */
