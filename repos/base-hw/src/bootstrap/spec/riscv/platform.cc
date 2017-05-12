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
		struct Uie    : Bitfield<0, 1> { };
		struct Sie    : Bitfield<1, 1> { };
		struct Upie   : Bitfield<4, 1> { };
		struct Spie   : Bitfield<5, 1> { };
		struct Spp    : Bitfield<8, 1> { };
		struct Mpp    : Bitfield<11, 2> { };


		struct Fs    : Bitfield<13, 2> { enum { INITIAL = 1 }; };
		struct Vm    : Bitfield<24, 5> { };
	};

	/* read status register */
	Mstatus::access_t mstatus = 0;

	Mstatus::Vm::set(mstatus, Mstatus::Sv39);        /* enable Sv39 paging  */
	Mstatus::Fs::set(mstatus, Mstatus::Fs::INITIAL); /* enable FPU          */
	Mstatus::Upie::set(mstatus, 1);                  /* user mode interrupt */
	Mstatus::Spp::set(mstatus, Mstatus::USER);       /* set user mode       */
	Mstatus::Spie::set(mstatus, 0);                  /* disable interrupts  */
	Mstatus::Mpp::set(mstatus, Mstatus::SUPERVISOR); /* set supervisor mode */

	asm volatile (
	              "la  t0,       1f\n"
	              "csrw sepc,    t0\n"
	              "csrw sptbr,   %0\n" /* set asid | page table */
	              "csrw mstatus, %1\n" /* change mode           */
	              "mret            \n" /* supverisor mode, jump to 1f */
	              "rdtime t0     \n"
	              "1:              \n"
	              :
	              : "r" ((addr_t)core_pd->table_base >> 12),
	                "r" (mstatus)
	              :  "memory");
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
