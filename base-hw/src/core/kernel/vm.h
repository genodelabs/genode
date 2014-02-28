/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__VM_H_
#define _KERNEL__VM_H_

/* Genode includes */
#include <cpu/cpu_state.h>

/* core includes */
#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>
#include <kernel/multiprocessor.h>
#include <kernel/signal_receiver.h>
#include <cpu.h>

namespace Kernel
{
	/**
	 * Kernel backend for a virtual machine
	 */
	class Vm;

	class Vm_ids : public Id_allocator<MAX_VMS> { };
	typedef Object_pool<Vm> Vm_pool;

	Vm_ids  * vm_ids();
	Vm_pool * vm_pool();
}

class Kernel::Vm : public Object<Vm, MAX_VMS, Vm_ids, vm_ids, vm_pool>,
                   public Execution_context
{
	private:

		struct Vm_state : Genode::Cpu_state_modes
		{
			Genode::addr_t dfar;
		};

		Vm_state       * const _state;
		Signal_context * const _context;

	public:

		/**
		 * Constructor
		 *
		 * \param state    initial CPU state
		 * \param context  signal for VM exceptions other than interrupts
		 */
		Vm(void           * const state,
		   Signal_context * const context)
		:
			Execution_context(multiprocessor()->primary(), Priority::MIN),
			_state((Vm_state * const)state),
			_context(context)
		{ }


		/****************
		 ** Vm_session **
		 ****************/

		void run()   { Execution_context::_schedule(); }

		void pause() { Execution_context::_unschedule(); }


		/***********************
		 ** Execution_context **
		 ***********************/

		void handle_exception(unsigned const processor_id)
		{
			switch(_state->cpu_exception) {
			case Genode::Cpu_state::INTERRUPT_REQUEST:
			case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
				_interrupt(processor_id);
				return;
			case Genode::Cpu_state::DATA_ABORT:
				_state->dfar = Genode::Cpu::Dfar::read();
			default:
				Execution_context::_unschedule();
				_context->submit(1);
			}
		}

		void proceed(unsigned const processor_id)
		{
			mtc()->continue_vm(_state, processor_id);
		}
};

#endif /* _KERNEL__VM_H_ */
