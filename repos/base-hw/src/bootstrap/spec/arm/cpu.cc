/*
 * \brief   Generic MMU initialization for ARM
 * \author  Stefan Kalkowski
 * \date    2017-04-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <spec/arm/cpu.h>

void Bootstrap::Cpu::enable_mmu_and_caches(Genode::addr_t table)
{
	/* invalidate TLB */
	Tlbiall::write(0);

	/* address space ID to zero */
	Cidr::write(0);

	/* do not use domains, but permission bits in table */
	Dacr::write(Dacr::D0::bits(1));

	Ttbcr::write(0);

	Ttbr::access_t ttbr0 = Ttbr::Ba::masked(table);
	Ttbr::Rgn::set(ttbr0, Ttbr::CACHEABLE);
	if (Mpidr::read()) { /* check for SMP system */
		Ttbr::Irgn::set(ttbr0, Ttbr::CACHEABLE);
		Ttbr::S::set(ttbr0, 1);
	} else
		Ttbr::C::set(ttbr0, 1);
	Ttbr0::write(ttbr0);

	Sctlr::access_t sctlr = Sctlr::read();
	Sctlr::C::set(sctlr, 1);
	Sctlr::I::set(sctlr, 1);
	Sctlr::V::set(sctlr, 1);
	Sctlr::A::set(sctlr, 0);
	Sctlr::M::set(sctlr, 1);
	Sctlr::Z::set(sctlr, 1);
	Sctlr::write(sctlr);

	/* invalidate branch predictor */
	Bpiall::write(0);
}
