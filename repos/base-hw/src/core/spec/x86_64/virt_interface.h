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

#include <cpu/vcpu_state.h>

using Genode::addr_t;
using Genode::Vcpu_state;

namespace Board
{
	struct Vcpu_state;

	enum class Virt_type { NONE, SVM, VMX };

	struct Virt_interface;
}


struct Board::Virt_interface
{
	Vcpu_state &vcpu_state;

	virtual void initialize(Board::Cpu &, addr_t) {};
	virtual void load(Genode::Vcpu_state &) {};
	virtual void store(Genode::Vcpu_state &) {};
	virtual void switch_world(Board::Cpu::Context &, addr_t) {};
	virtual Virt_type virt_type() { return Virt_type::NONE; };
	virtual Genode::uint64_t handle_vm_exit() { return 0; };

	Virt_interface(Vcpu_state &state) : vcpu_state(state) { }

	virtual ~Virt_interface() = default;
};

#endif /* _INCLUDE__SPEC__PC__VIRT_INTERFACE_H_ */
