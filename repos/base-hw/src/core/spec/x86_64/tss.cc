/*
 * \brief  64-bit Task State Segment (TSS)
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <mtc_util.h>

#include "tss.h"

using namespace Genode;

extern int _mt_kernel_interrupt_stack;

void Tss::setup(addr_t const virt_base)
{
	addr_t const stack_addr = _virt_mtc_addr(virt_base,
			(addr_t)&_mt_kernel_interrupt_stack);

	this->rsp0 = stack_addr;
	this->rsp1 = stack_addr;
	this->rsp2 = stack_addr;
}


void Tss::load()
{
	asm volatile ("ltr %w0" : : "r" (TSS_SELECTOR));
}
