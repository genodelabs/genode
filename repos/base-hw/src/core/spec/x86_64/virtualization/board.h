/*
 * \brief  Board with PC virtualization support
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_
#define _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_

/* base-hw core includes */
#include <kernel/configuration.h>
#include <kernel/irq.h>

#include <cpu.h>
#include <cpu/vcpu_state_virtualization.h>
#include <hw/spec/x86_64/x86_64.h>
#include <spec/x86_64/virtualization/ept.h>
#include <spec/x86_64/virtualization/svm.h>
#include <spec/x86_64/virtualization/vmx.h>

using Genode::addr_t;
using Genode::uint64_t;

namespace Board {

	using Vm_page_table = Hw::Ept;
	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;
	struct Vcpu_context;
	using Vcpu_data = Genode::Vcpu_data;
	using Vcpu_state = Genode::Vcpu_state;

	enum {
		VCPU_MAX = 16
	};

	enum Platform_exitcodes : uint64_t {
		EXIT_NPF     = 0xfc,
		EXIT_INIT    = 0xfd,
		EXIT_STARTUP = 0xfe,
		EXIT_PAUSED  = 0xff,
	};

	enum Custom_trapnos : uint64_t {
		TRAP_VMEXIT = 256,
		TRAP_VMSKIP = 257,
	};
};


namespace Kernel {
	class Cpu;
	class Vm;
};


struct Board::Vcpu_context
{
	Vcpu_context(unsigned id, Vcpu_data &vcpu_data);
	void initialize(Kernel::Cpu &cpu, addr_t table_phys_addr);
	void read_vcpu_state(Vcpu_state &state);
	void write_vcpu_state(Vcpu_state &state);

	Genode::Align_at<Core::Cpu::Context> regs;

	Virt_interface                      &virt;

	uint64_t tsc_aux_host = 0U;
	uint64_t tsc_aux_guest = 0U;
	uint64_t exit_reason = EXIT_INIT;

	static Virt_interface &detect_virtualization(Vcpu_data &vcpu_data,
	                                             unsigned   id)
	{
		if (Hw::Virtualization_support::has_svm())
			return *Genode::construct_at<Vmcb>(
				vcpu_data.virt_area,
				vcpu_data,
				id);
		else if (Hw::Virtualization_support::has_vmx()) {
			return *Genode::construct_at<Vmcs>(
				vcpu_data.virt_area,
				vcpu_data);
		} else {
			Genode::error( "No virtualization support detected.");
			throw Core::Service_denied();
		}
	}
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_ */
