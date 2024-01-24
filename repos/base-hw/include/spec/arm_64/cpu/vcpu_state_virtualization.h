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

#ifndef _INCLUDE__SPEC__ARM_64__CPU__VM_STATE_VIRTUALIZATION_H_
#define _INCLUDE__SPEC__ARM_64__CPU__VM_STATE_VIRTUALIZATION_H_

/* Genode includes */
#include <cpu/cpu_state.h>

namespace Genode {

	enum { VCPU_EXCEPTION_STARTUP = 0xfe };

	/**
	 * CPU context of a virtual machine
	 */
	struct Vcpu_state;
	using Vcpu_data = Vcpu_state;

	using uint128_t = __uint128_t;
}


struct Genode::Vcpu_state : Genode::Cpu_state
{
	Genode::uint64_t pstate         { 0 };
	Genode::uint64_t exception_type { 0 };
	Genode::uint64_t esr_el2        { 0 };

	/** Fpu registers - q must start at 16-byte aligned offset **/
	Genode::uint32_t fpcr           { 0 };
	Genode::uint32_t fpsr           { 0 };
	Genode::uint128_t q[32]         { 0 };

	Genode::uint64_t elr_el1        { 0 };
	Genode::uint64_t sp_el1         { 0 };
	Genode::uint64_t spsr_el1       { 0 };

	Genode::uint64_t sctlr_el1      { 0 };
	Genode::uint64_t actlr_el1      { 0 };
	Genode::uint64_t vbar_el1       { 0 };
	Genode::uint32_t cpacr_el1      { 0 };
	Genode::uint32_t afsr0_el1      { 0 };
	Genode::uint32_t afsr1_el1      { 0 };
	Genode::uint32_t contextidr_el1 { 0 };

	Genode::uint64_t ttbr0_el1      { 0 };
	Genode::uint64_t ttbr1_el1      { 0 };
	Genode::uint64_t tcr_el1        { 0 };
	Genode::uint64_t mair_el1       { 0 };
	Genode::uint64_t amair_el1      { 0 };
	Genode::uint64_t far_el1        { 0 };
	Genode::uint64_t par_el1        { 0 };

	Genode::uint64_t tpidrro_el0    { 0 };
	Genode::uint64_t tpidr_el0      { 0 };
	Genode::uint64_t tpidr_el1      { 0 };

	Genode::uint64_t vmpidr_el2     { 0 };

	Genode::uint64_t far_el2        { 0 };
	Genode::uint64_t hpfar_el2      { 0 };

	/**
	 * Timer related registers
	 */
	struct Timer {
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

	/**************************
	 ** Platform information **
	 **************************/

	Genode::uint64_t id_aa64isar0_el1 { 0 };
	Genode::uint64_t id_aa64isar1_el1 { 0 };
	Genode::uint64_t id_aa64mmfr0_el1 { 0 };
	Genode::uint64_t id_aa64mmfr1_el1 { 0 };
	Genode::uint64_t id_aa64mmfr2_el1 { 0 };
	Genode::uint64_t id_aa64pfr0_el1  { 0 };
	Genode::uint64_t id_aa64pfr1_el1  { 0 };
	Genode::uint64_t id_aa64zfr0_el1  { 0 };

	Genode::uint32_t ccsidr_inst_el1[7] { 0 };
	Genode::uint32_t ccsidr_data_el1[7] { 0 };
	Genode::uint64_t clidr_el1          { 0 };
};

#endif /* _INCLUDE__SPEC__ARM_64__CPU__VM_STATE_VIRTUALIZATION_H_ */
