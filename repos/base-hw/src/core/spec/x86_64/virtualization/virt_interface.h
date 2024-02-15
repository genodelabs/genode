/*
 * \brief   Virtualization interface
 * \author  Benjamin Lamowski
 * \date    2024-02-15
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PC__VIRT_INTERFACE_H_
#define _INCLUDE__SPEC__PC__VIRT_INTERFACE_H_

#include <cpu.h>
#include <cpu/vcpu_state.h>
#include <cpu/vcpu_state_virtualization.h>

using Genode::addr_t;
using Genode::Vcpu_state;

namespace Kernel {
	class Cpu;
	class Vm;
};

namespace Board
{
enum Virt_type {
	SVM,
};

struct Virt_interface
{
	Genode::Vcpu_data &vcpu_data;

	virtual void initialize(Kernel::Cpu        &cpu,
	                        addr_t              page_table_phys_addr,
				Core::Cpu::Context &regs)   = 0;
	virtual void write_vcpu_state(Vcpu_state &state)    = 0;
	virtual void read_vcpu_state(Vcpu_state &state)     = 0;
	virtual void switch_world(Core::Cpu::Context &regs) = 0;
	virtual Virt_type virt_type()                       = 0;
	virtual Genode::uint64_t handle_vm_exit()           = 0;

	Virt_interface(Genode::Vcpu_data &vcpu_data) : vcpu_data(vcpu_data)
	{ }

	virtual ~Virt_interface() = default;
};
} /* namespace Board */

#endif /* _INCLUDE__SPEC__PC__VIRT_INTERFACE_H_ */
