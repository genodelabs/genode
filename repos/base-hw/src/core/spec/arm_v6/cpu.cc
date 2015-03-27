/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <cpu.h>
#include <kernel/pd.h>

void Genode::Arm::flush_data_caches() {
	asm volatile ("mcr p15, 0, %[rd], c7, c14, 0" :: [rd]"r"(0) : ); }

void Genode::Arm::invalidate_data_caches() {
	asm volatile ("mcr p15, 0, %[rd], c7, c6, 0" :: [rd]"r"(0) : ); }


void Genode::Cpu::init_virt_kernel(Kernel::Pd* pd)
{
	Cidr::write(pd->asid);
	Dacr::write(Dacr::init_virt_kernel());
	Ttbr0::write(Ttbr0::init((Genode::addr_t)pd->translation_table()));
	Ttbcr::write(0);
	Sctlr::write(Sctlr::init_virt_kernel());
}
