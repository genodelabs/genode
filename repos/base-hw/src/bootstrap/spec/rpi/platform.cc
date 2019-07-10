/*
 * \brief   Platform implementations specific for base-hw and Raspberry Pi
 * \author  Norman Feske
 * \author  Stefan Kalkowski
 * \date    2013-04-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <hw/assert.h>
#include <platform.h>

using namespace Rpi;

/**
 * Leave out the first page (being 0x0) from bootstraps RAM allocator,
 * some code does not feel happy with addresses being zero
 */
Bootstrap::Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE + 0x1000,
                                    RAM_0_SIZE - 0x1000 }),
  late_ram_regions(Memory_region { RAM_0_BASE, 0x1000 }),
  core_mmio(Memory_region { PL011_0_MMIO_BASE,
                            PL011_0_MMIO_SIZE },
            Memory_region { SYSTEM_TIMER_MMIO_BASE,
                            SYSTEM_TIMER_MMIO_SIZE },
            Memory_region { IRQ_CONTROLLER_BASE,
                            IRQ_CONTROLLER_SIZE },
            Memory_region { USB_DWC_OTG_BASE,
                            USB_DWC_OTG_SIZE }) {}


unsigned Bootstrap::Platform::enable_mmu()
{
	using ::Board::Cpu;

	struct Sctlr : Cpu::Sctlr
	{
		struct W  : Bitfield<3,1>  { }; /* enable write buffer */
		struct Dt : Bitfield<16,1> { }; /* global data TCM enable */
		struct It : Bitfield<18,1> { }; /* global instruction TCM enable */
		struct U  : Bitfield<22,1> { }; /* enable unaligned data access */
		struct Xp : Bitfield<23,1> { }; /* disable subpage AP bits */
	};

	Cpu::Sctlr::init();
	Cpu::Sctlr::access_t sctlr = Cpu::Sctlr::read();
	Sctlr::W::set(sctlr, 1);
	Sctlr::Dt::set(sctlr, 1);
	Sctlr::It::set(sctlr, 1);
	Sctlr::U::set(sctlr, 1);
	Sctlr::Xp::set(sctlr, 1);
	Cpu::Sctlr::write(sctlr);

	Cpu::Cpsr::init();

	struct Ctr : Cpu::Ctr {
		struct P : Bitfield<23, 1> { }; /* page mapping restriction on */
	};

	/* check for mapping restrictions */
	assert(!Ctr::P::get(Cpu::Ctr::read()));

	/* invalidate TLB */
	Cpu::Tlbiall::write(0);

	/* address space ID to zero */
	Cpu::Cidr::write(0);

	/* do not use domains, but permission bits in table */
	Cpu::Dacr::write(Cpu::Dacr::D0::bits(1));

	Cpu::Ttbcr::write(1);

	Genode::addr_t table = (Genode::addr_t)core_pd->table_base;
	Cpu::Ttbr::access_t ttbr = Cpu::Ttbr::init(table);
	Cpu::Ttbr0::write(ttbr);
	Cpu::Ttbr1::write(ttbr);

	sctlr = Cpu::Sctlr::read();
	Cpu::Sctlr::C::set(sctlr, 1);
	Cpu::Sctlr::I::set(sctlr, 1);
	Cpu::Sctlr::M::set(sctlr, 1);
	Cpu::Sctlr::write(sctlr);

	/* invalidate branch predictor */
	Cpu::Bpiall::write(0);

	return 0;
}
