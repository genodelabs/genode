/*
 * \brief  CPU state
 * \author Christian Prochaska
 * \date   2011-04-15
 *
 * This file contains the generic part of the CPU state.
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CPU_STATE_H_
#define _INCLUDE__BASE__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
	{
		addr_t ip;   /* instruction pointer */
		addr_t sp;   /* stack pointer       */

		/**
		 * Constructor
		 */
		Cpu_state(): ip(0), sp(0) { }
	};
}

#endif /* _INCLUDE__BASE__CPU_STATE_H_ */
