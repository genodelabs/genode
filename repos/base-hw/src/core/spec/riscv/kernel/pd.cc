/*
 * \brief   Kernel pd object implementations for RiscV
 * \author  Stefan Kalkowski
 * \date    2018-11-22
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/pd.h>

bool Kernel::Pd::invalidate_tlb(Kernel::Cpu&, addr_t, size_t)
{
	Genode::Cpu::sfence();
	return false;
}


