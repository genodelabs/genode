/*
 * \brief  Board wirh ARM virtualization support
 * \author Stefan Kalkowski
 * \date   2019-11-12
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__VIRTUALIZATION__BOARD_H_
#define _CORE__SPEC__ARM__VIRTUALIZATION__BOARD_H_

#include <base/ram_allocator.h>

/* core includes */
#include <kernel/configuration.h>
#include <kernel/irq.h>
#include <core_ram.h>

/* base-hw internal includes */
#include <hw/spec/arm/lpae.h>

namespace Genode { struct Vcpu_state; }

namespace Board {

	using Vm_page_table = Hw::Level_1_stage_2_translation_table;
	using Vm_page_table_array = Hw::Page_table::Array;

	struct Vcpu_context;
	struct Vcpu_state;
};


namespace Kernel {
	class Cpu;
	class Vcpu;
};


struct Board::Vcpu_context
{
	class Vm_irq : public Kernel::Irq
	{
		private:

			Kernel::Cpu &_cpu;

		public:

			Vm_irq(unsigned const irq, Kernel::Cpu &cpu);

			virtual ~Vm_irq() { };

			virtual void handle(Kernel::Vcpu &vcpu, unsigned irq);

			void occurred() override;
	};


	struct Maintainance_irq : Vm_irq
	{
		Maintainance_irq(Kernel::Cpu &);

		void handle(Kernel::Vcpu&, unsigned) override { }
	};


	struct Virtual_timer_irq
	{
		Vm_irq irq;

		Virtual_timer_irq(Kernel::Cpu &);

		void enable();
		void disable();
	};

	Vcpu_context(Kernel::Cpu &cpu)
	: maintainance_irq(cpu), vtimer_irq(cpu) {}

	Local_interrupt_controller::Virtual_context ic_context {};
	Maintainance_irq maintainance_irq;
	Virtual_timer_irq    vtimer_irq;
};


class Board::Vcpu_state
{
	private:

		Genode::Vcpu_state *_state { nullptr };

	public:

		/* construction for this architecture always successful */
		using Error = Core::Accounted_mapped_ram_allocator::Error;
		using Constructed = Genode::Attempt<Genode::Ok, Error>;
		Constructed const constructed = Genode::Ok();

		/*
		 * Noncopyable
		 */
		Vcpu_state(Vcpu_state const &) = delete;
		Vcpu_state &operator = (Vcpu_state const &) = delete;

		Vcpu_state(Core::Accounted_mapped_ram_allocator &,
		           Core::Local_rm &,
		           Genode::Vcpu_state *state)
		: _state(state) {}

		void with_state(auto const fn) {
			if (_state != nullptr) fn(*_state); }
};

#endif /* _CORE__SPEC__ARM__VIRTUALIZATION__BOARD_H_ */
