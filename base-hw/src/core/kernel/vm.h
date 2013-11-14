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
#include <kernel/signal_receiver.h>

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

		Genode::Cpu_state_modes * const _state;
		Signal_context * const          _context;

	public:

		/**
		 * Constructor
		 *
		 * \param state    initial CPU state
		 * \param context  signal for VM exceptions other than interrupts
		 */
		Vm(Genode::Cpu_state_modes * const state,
		   Signal_context * const context)
		:
			_state(state), _context(context)
		{
			/* set VM to least priority by now */
			priority = 0;
		}


		/****************
		 ** Vm_session **
		 ****************/

		void run()   { cpu_scheduler()->insert(this); }

		void pause() { cpu_scheduler()->remove(this); }


		/***********************
		 ** Execution_context **
		 ***********************/

		void handle_exception()
		{
			switch(_state->cpu_exception) {
			case Genode::Cpu_state::INTERRUPT_REQUEST:
			case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
				handle_interrupt();
				return;
			default:
				cpu_scheduler()->remove(this);
				_context->submit(1);
			}
		}

		void proceed() { mtc()->continue_vm(_state); }
};

#endif /* _KERNEL__VM_H_ */
