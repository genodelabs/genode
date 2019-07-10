/*
 * \brief   MMU initialization for Cortex A15
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

void Board::Cpu::enable_mmu_and_caches(Genode::addr_t table)
{
	/* invalidate TLB */
	Tlbiall::write(0);

	enum Memory_attributes {
		DEVICE_MEMORY          = 0x04,
		NORMAL_MEMORY_UNCACHED = 0x44,
		NORMAL_MEMORY_CACHED   = 0xff,
	};

	/* set memory attributes in indirection register */
	Mair0::access_t mair0 = 0;
	Mair0::Attr0::set(mair0, NORMAL_MEMORY_UNCACHED);
	Mair0::Attr1::set(mair0, DEVICE_MEMORY);
	Mair0::Attr2::set(mair0, NORMAL_MEMORY_CACHED);
	Mair0::Attr3::set(mair0, DEVICE_MEMORY);
	Mair0::write(mair0);

	/* do not use domains, but permission bits in table */
	Dacr::write(Dacr::D0::bits(1));

	Ttbr_64bit::access_t ttbr0 = Ttbr_64bit::Ba::masked(table);
	Ttbr_64bit::access_t ttbr1 = Ttbr_64bit::Ba::masked(table);
	Ttbr_64bit::Asid::set(ttbr0, 0);
	Ttbr0_64bit::write(ttbr0);
	Ttbr1_64bit::write(ttbr1);

	Ttbcr::access_t ttbcr = 0;
	Ttbcr::T0sz::set(ttbcr, 1);
	Ttbcr::T1sz::set(ttbcr, 0);
	Ttbcr::Irgn0::set(ttbcr, 1);
	Ttbcr::Irgn1::set(ttbcr, 1);
	Ttbcr::Orgn0::set(ttbcr, 1);
	Ttbcr::Orgn1::set(ttbcr, 1);
	Ttbcr::Sh0::set(ttbcr, 0b10);
	Ttbcr::Sh1::set(ttbcr, 0b10);
	Ttbcr::Eae::set(ttbcr, 1);
	Ttbcr::write(ttbcr);

	/* toggle smp bit */
	Actlr::access_t actlr = Actlr::read();
	Actlr::write(actlr | (1 << 6));

	Sctlr::access_t sctlr = Sctlr::read();
	Sctlr::C::set(sctlr, 1);
	Sctlr::I::set(sctlr, 1);
	Sctlr::V::set(sctlr, 1);
	Sctlr::M::set(sctlr, 1);
	Sctlr::Z::set(sctlr, 1);
	Sctlr::write(sctlr);

	/* invalidate branch predictor */
	Bpiall::write(0);
}
