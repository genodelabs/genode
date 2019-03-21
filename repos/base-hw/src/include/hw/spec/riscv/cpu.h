/*
 * \brief  CPU definitions for RiscV
 * \author Stefan Kalkowski
 * \date   2017-09-14
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__CPU_H_
#define _SRC__LIB__HW__SPEC__RISCV__CPU_H_

#include <hw/spec/riscv/register_macros.h>

namespace Hw { struct Riscv_cpu; }

struct Hw::Riscv_cpu
{
	/**************************
	 ** Supervisor registers **
	 **************************/

	/* Supervisor-mode status Register */
	RISCV_SUPERVISOR_REGISTER(Sstatus, sstatus,
		struct Spp : Bitfield<8,1>  { }; /* prior privilege level */
	);

	/* Supervisor Trap Vector Base Address Register */
	RISCV_SUPERVISOR_REGISTER(Stvec, stvec);

	/* Supervisor Bad Address Register */
	RISCV_SUPERVISOR_REGISTER(Sbadaddr, sbadaddr);

	/* Supervisor Page-Table Base Register */
	RISCV_SUPERVISOR_REGISTER(Sptbr, sptbr,
		struct Ppn  : Bitfield<0, 38> { };
		struct Asid : Bitfield<38, 26> { };
	);
};

#endif /* _SRC__LIB__HW__SPEC__RISCV__CPU_H_ */

