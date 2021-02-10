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
		struct Sie : Bitfield<1,1>   { }; /* supervisor interrupt enable */
		struct Spp : Bitfield<8,1>  { }; /* prior privilege level */
	);

	/* Supervisor Trap Vector Base Address Register */
	RISCV_SUPERVISOR_REGISTER(Stvec, stvec);

	/* Supervisor Trap Value (replaces Sbadaddr in ISA 1.10) */
	RISCV_SUPERVISOR_REGISTER(Stval, stval);

	/* Supervisor address translation an protection (replaces sptbr in ISA 1.10) */
	RISCV_SUPERVISOR_REGISTER(Satp, satp,
		struct Ppn  : Bitfield<0, 44> { };
		struct Asid : Bitfield<44,16> { };
		struct Mode : Bitfield<60, 4> { };
	);
};

#endif /* _SRC__LIB__HW__SPEC__RISCV__CPU_H_ */

