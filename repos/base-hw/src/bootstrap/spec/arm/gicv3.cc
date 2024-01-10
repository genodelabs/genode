/*
 * \brief  GICv3 interrupt controller for core
 * \author Sebastian Sumpf
 * \date   2019-07-08
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>

Hw::Pic::Pic()
:
	_distr({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_BASE,
	                Board::Cpu_mmio::IRQ_CONTROLLER_DISTR_SIZE}),
	_redistr({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE,
	                  Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE}),
	_redistr_sgi({(char *)Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE +
	                      Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE / 2,
	                      Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_BASE -
	                      Board::Cpu_mmio::IRQ_CONTROLLER_REDIST_SIZE / 2}),
	_max_irq(_distr.max_irq())
{
	/* disable device */
	_distr.write<Distributor::Ctlr>(0);
	_distr.wait_for_rwp();

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
	for (unsigned i = min_spi; i <= _max_irq; i++) {
		_distr.write<Distributor::Icfgr::Edge_triggered>(0, i);
		_distr.write<Distributor::Ipriorityr::Priority>(0xa0, i);
		_distr.write<Distributor::Icenabler::Clear_enable>(1, i);
		_distr.write<Distributor::Icpendr::Clear_pending>(1, i);
		_distr.write<Distributor::Igroup0r::Group1>(1, i);

		/* XXX remove: route all SPIs to this PE */
		_distr.write<Distributor::Irouter>(affinity, i);
	}

	/* enable device GRP1_NS with affinity */
	Distributor::Ctlr::access_t ctlr = 0;
	Distributor::Ctlr::Enable_grp1_a::set(ctlr, 1);
	Distributor::Ctlr::Are_ns::set(ctlr, 1);

	_distr.write<Distributor::Ctlr>(ctlr);
	_distr.wait_for_rwp();
}
