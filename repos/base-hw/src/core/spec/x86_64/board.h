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

#include <session/session.h>

#include <cpu/vcpu_state.h>
#include <hw/spec/x86_64/x86_64.h>
#include <spec/x86_64/svm.h>
#include <spec/x86_64/vmx.h>
#include <phys_allocated.h>

namespace Board {
	using Genode::addr_t;
	using Genode::uint64_t;

	struct Virt_interface;
	struct Vcpu_context;
	class  Vcpu_state;

	enum Platform_exitcodes : uint64_t {
		EXIT_NPF     = 0xfc,
		EXIT_PAUSED  = 0xff,
	};

	enum Custom_trapnos : uint64_t {
		TRAP_VMEXIT = 256,
		TRAP_VMDEAD = 257,
	};
};


class Board::Vcpu_state
{
	private:

		Core::Local_rm &_local_rm;

		struct Vm_hw_context { uint8_t _[get_page_size()*3]; };

		Core::Phys_allocated<Vm_hw_context> _hw_context;

		/**
		 * physical address of hardware context needed by kernel,
		 * cannot be called by kernel directly for now, therefore
		 * cache it in this variable
		 */
		addr_t _hw_context_phys_addr { _hw_context.phys_addr() };

		Genode::Vcpu_state *_state { nullptr };

	public:

		/*
		 * Noncopyable
		 */
		Vcpu_state(Vcpu_state const &) = delete;
		Vcpu_state &operator = (Vcpu_state const &) = delete;

		Vcpu_state(Rpc_entrypoint          &ep,
		           Accounted_ram_allocator &ram,
		           Core::Local_rm          &local_rm,
		           Ram_allocator::Result   &ds);
		~Vcpu_state();

		addr_t vmc_addr() const { return (addr_t)&_hw_context.obj; };
		addr_t vmc_phys_addr() const { return _hw_context_phys_addr; }

		void with_state(auto const fn) {
			if (_state != nullptr) fn(*_state); }
};


struct Board::Vcpu_context
{
	enum class Init_state {
		CREATED,
		STARTED
	};

	Vcpu_context(unsigned id, Vcpu_state &vcpu_data);
	void initialize(Board::Cpu &cpu, addr_t table_phys_addr);
	void load(Genode::Vcpu_state &state);
	void store(Genode::Vcpu_state &state);

	Genode::Align_at<Board::Cpu::Context> regs;

	Virt_interface &virt;

	uint64_t tsc_aux_host = 0U;
	uint64_t tsc_aux_guest = 0U;
	uint64_t exit_reason = EXIT_PAUSED;

	Init_state init_state { Init_state::CREATED };

	static Virt_interface &detect_virtualization(Vcpu_state &, unsigned);
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_ */
