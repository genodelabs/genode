/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>
#include <cpu.h>

void Genode::Cpu::init_virt_kernel(Kernel::Pd * pd)
{
	Mair0::write(Mair0::init_virt_kernel());
	Dacr::write(Dacr::init_virt_kernel());
	Ttbr0::write(Ttbr0::init((Genode::addr_t)pd->translation_table(),
	                         pd->asid));
	Ttbcr::write(Ttbcr::init_virt_kernel());
	Sctlr::write(Sctlr::init_virt_kernel());
	inval_branch_predicts();
}
