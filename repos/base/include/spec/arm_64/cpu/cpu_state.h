/*
 * \brief  CPU state
 * \author Stefan Kalkowski
 * \date   2019-03-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__CPU__CPU_STATE_H_
#define _INCLUDE__SPEC__ARM_64__CPU__CPU_STATE_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { struct Cpu_state; }


struct Genode::Cpu_state
{
	addr_t r[31] { 0 }; /* general purpose register 0...30 */
	addr_t sp    { 0 }; /* stack pointer                   */
	addr_t ip    { 0 }; /* instruction pointer             */
};

#endif /* _INCLUDE__SPEC__ARM_64__CPU__CPU_STATE_H_ */

