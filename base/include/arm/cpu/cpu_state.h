/*
 * \brief  CPU state
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM__CPU__CPU_STATE_H_
#define _INCLUDE__ARM__CPU__CPU_STATE_H_

#include <base/stdint.h>

namespace Genode {

	struct Cpu_state
	{
		addr_t ip;
		addr_t sp;
		addr_t r[13];
		addr_t lr;
		addr_t cpsr;
	};
}

#endif /* _INCLUDE__ARM__CPU__CPU_STATE_H_ */
