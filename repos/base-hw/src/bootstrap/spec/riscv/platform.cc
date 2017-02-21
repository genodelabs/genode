/*
 * \brief   Platform implementations specific for RISC-V
 * \author  Stefan Kalkowski
 * \author  Sebastian Sumpf
 * \date    2016-11-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/string.h>

#include <platform.h>

Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { 0, 128 * 1024 * 1024 } ) {}


void Bootstrap::Platform::enable_mmu()
{
	struct Mstatus : Genode::Register<64>
	{
		enum {
			USER       = 0,
			SUPERVISOR = 1,
			Sv39       = 9,
		};
		struct Ie    : Bitfield<0, 1> { };
		struct Priv  : Bitfield<1, 2> { };
		struct Ie1   : Bitfield<3, 1> { };
		struct Priv1 : Bitfield<4, 2> { };
		struct Fs    : Bitfield<12, 2> { enum { INITIAL = 1 }; };
		struct Vm    : Bitfield<17, 5> { };
	};

	/* read status register */
	Mstatus::access_t mstatus = 0;

	Mstatus::Vm::set(mstatus, Mstatus::Sv39);         /* enable Sv39 paging  */
	Mstatus::Fs::set(mstatus, Mstatus::Fs::INITIAL);  /* enable FPU          */
	Mstatus::Ie1::set(mstatus, 1);                    /* user mode interrupt */
	Mstatus::Priv1::set(mstatus, Mstatus::USER);      /* set user mode       */
	Mstatus::Ie::set(mstatus, 0);                     /* disable interrupts  */
	Mstatus::Priv::set(mstatus, Mstatus::SUPERVISOR); /* set supervisor mode */

	asm volatile ("csrw sasid,   %0\n" /* address space id  */
	              "csrw sptbr,   %1\n" /* set page table    */
	              "csrw mstatus, %2\n" /* change mode       */
	              :
	              : "r" (0/*core_pd.asid*/),
	                "r" (core_pd->table_base),
	                "r" (mstatus)
	              : "memory");
}


extern int _machine_begin, _machine_end;

extern "C" void setup_riscv_exception_vector()
{
	using namespace Genode;

	/* retrieve exception vector */
	addr_t vector;
	asm volatile ("csrr %0, mtvec\n" : "=r"(vector));

	/* copy  machine mode exception vector */
	memcpy((void *)vector,
	       &_machine_begin, (addr_t)&_machine_end - (addr_t)&_machine_begin);
}
