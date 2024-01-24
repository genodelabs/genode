/*
 * \brief  CPU, PIC, and timer context of a virtual machine
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2015-02-10
 */

/*
 * Copyright (C) 2015-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARNDALE__VM_STATE_H_
#define _INCLUDE__SPEC__ARNDALE__VM_STATE_H_

/* Genode includes */
#include <cpu/cpu_state.h>

namespace Genode {

	enum { VCPU_EXCEPTION_STARTUP = 0xfe };

	/**
	 * CPU context of a virtual machine
	 */
	struct Vcpu_state;
	using Vcpu_data = Vcpu_state;
}


struct Genode::Vcpu_state : Genode::Cpu_state_modes
{
	Genode::uint64_t vttbr     { 0 };
	Genode::uint32_t sctrl     { 0 };
	Genode::uint32_t esr_el2   { 0 };
	Genode::uint32_t hpfar_el2 { 0 };
	Genode::uint32_t far_el2   { 0 };
	Genode::uint32_t hifar     { 0 };
	Genode::uint32_t ttbcr     { 0 };
	Genode::uint32_t ttbr0     { 0 };
	Genode::uint32_t ttbr1     { 0 };
	Genode::uint32_t prrr      { 0 };
	Genode::uint32_t nmrr      { 0 };
	Genode::uint32_t dacr      { 0 };
	Genode::uint32_t dfsr      { 0 };
	Genode::uint32_t ifsr      { 0 };
	Genode::uint32_t adfsr     { 0 };
	Genode::uint32_t aifsr     { 0 };
	Genode::uint32_t dfar      { 0 };
	Genode::uint32_t ifar      { 0 };
	Genode::uint32_t cidr      { 0 };
	Genode::uint32_t tls1      { 0 };
	Genode::uint32_t tls2      { 0 };
	Genode::uint32_t tls3      { 0 };
	Genode::uint32_t cpacr     { 0 };
	Genode::uint32_t vmpidr    { 0 };

	/**
	 * Fpu registers
	 */
	Genode::uint32_t fpscr     { 0 };
	Genode::uint64_t d0_d31[32]{ 0 };

	/**
	 * Timer related registers
	 */
	struct Timer
	{
		Genode::uint64_t offset   { 0 };
		Genode::uint64_t compare  { 0 };
		Genode::uint32_t control  { 0 };
		Genode::uint32_t kcontrol { 0 };
		bool             irq      { false };
	} timer {};

	/**
	 * Interrupt related values
	 */
	struct Pic
	{
		unsigned last_irq    { 1023 };
		unsigned virtual_irq { 1023 };
	} irqs {};
};

#endif /* _INCLUDE__SPEC__ARNDALE__VM_STATE_H_ */
