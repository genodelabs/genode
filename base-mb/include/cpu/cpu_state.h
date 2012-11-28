/*
 * \brief  CPU state
 * \author Martin Stein
 * \date   2012-11-26
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_MB__INCLUDE__CPU__CPU_STATE_H_
#define _BASE_MB__INCLUDE__CPU__CPU_STATE_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode {

	/**
	 * Basic CPU state
	 */
	struct Cpu_state
	{
		/**
		 * Registers
		 */
		addr_t sp; /* stack pointer */
		addr_t ip; /* instruction pointer */
	};
}

#endif /* _BASE_MB__INCLUDE__CPU__CPU_STATE_H_ */

