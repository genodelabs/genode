/*
 * \brief  GICv3 interrupt controller for core
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2019-07-08
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

Hw::Global_interrupt_controller::Global_interrupt_controller()
:
	Mmio({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE,
	      Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_SIZE})
{
	/* disable device */
	write<Ctlr>(0);
	wait_for_rwp();

	/* XXX: remove */
	struct Affinity : Genode::Register<64>
	{
		struct Aff0 : Bitfield<0, 8> { };
		struct Aff1 : Bitfield<8, 8> { };
		struct Aff2 : Bitfield<16, 8> { };
		struct Aff3 : Bitfield<32, 8> { };
	};
	Genode::uint64_t mpidr = 0;
	asm volatile ("mrs %0, mpidr_el1" : "=r"(mpidr) : : "memory");
	Affinity::access_t affinity = 0;
	Affinity::Aff0::set(affinity, mpidr);
	Affinity::Aff1::set(affinity, (mpidr >> 8) & 0xff);
	Affinity::Aff2::set(affinity, (mpidr >> 16) & 0xff);
	Affinity::Aff3::set(affinity, (mpidr >> 32) & 0xff);

	/* configure every shared peripheral interrupt */
	for (unsigned i = MIN_SPI; i <= max_irq(); i++) {
		write<Icfgr::Edge_triggered>(0, i);
		write<Ipriorityr::Priority>(0xa0, i);
		write<Icenabler::Clear_enable>(1, i);
		write<Icpendr::Clear_pending>(1, i);
		write<Igroup0r::Group1>(1, i);

		/* XXX remove: route all SPIs to this PE */
		write<Irouter>(affinity, i);
	}

	/* enable device GRP1_NS with affinity */
	Ctlr::access_t ctlr = 0;
	Ctlr::Enable_grp1_a::set(ctlr, 1);
	Ctlr::Are_ns::set(ctlr, 1);

	write<Ctlr>(ctlr);
	wait_for_rwp();
}


Hw::Local_interrupt_controller::Local_interrupt_controller(Global_interrupt_controller &gic)
:
	_distr(gic),
	_redistr({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE,
	                  Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE}),
	_redistr_sgi({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE +
	                      Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE / 2,
	                      Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE -
	                      Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE / 2}),
	_max_irq(_distr.max_irq()) { }
