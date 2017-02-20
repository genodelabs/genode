/*
 * \brief   CPU, PIC, and timer context of a virtual machine
 * \author  Stefan Kalkowski
 * \date    2015-02-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARNDALE__VM_STATE_H_
#define _INCLUDE__SPEC__ARNDALE__VM_STATE_H_

/* Genode includes */
#include <cpu/cpu_state.h>

namespace Genode
{
	/**
	 * CPU context of a virtual machine
	 */
	struct Vm_state;
}

struct Genode::Vm_state : Genode::Cpu_state_modes
{
	Genode::uint64_t vttbr;
	Genode::uint32_t sctrl;
	Genode::uint32_t hsr;
	Genode::uint32_t hpfar;
	Genode::uint32_t hdfar;
	Genode::uint32_t hifar;
	Genode::uint32_t ttbcr;
	Genode::uint32_t ttbr0;
	Genode::uint32_t ttbr1;
	Genode::uint32_t prrr;
	Genode::uint32_t nmrr;
	Genode::uint32_t dacr;
	Genode::uint32_t dfsr;
	Genode::uint32_t ifsr;
	Genode::uint32_t adfsr;
	Genode::uint32_t aifsr;
	Genode::uint32_t dfar;
	Genode::uint32_t ifar;
	Genode::uint32_t cidr;
	Genode::uint32_t tls1;
	Genode::uint32_t tls2;
	Genode::uint32_t tls3;
	Genode::uint32_t cpacr;


	/**
	 * Timer related registers
	 */

	Genode::uint32_t timer_ctrl;
	Genode::uint32_t timer_val;
	bool             timer_irq;


	/**
	 * PIC related registers
	 */

	enum { NR_IRQ = 4 };

	Genode::uint32_t gic_hcr;
	Genode::uint32_t gic_vmcr;
	Genode::uint32_t gic_misr;
	Genode::uint32_t gic_apr;
	Genode::uint32_t gic_eisr;
	Genode::uint32_t gic_elrsr0;
	Genode::uint32_t gic_lr[4];
	unsigned         gic_irq;
};

#endif /* _INCLUDE__SPEC__ARNDALE__VM_STATE_H_ */
