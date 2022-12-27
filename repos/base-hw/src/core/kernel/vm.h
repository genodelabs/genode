/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__VM_H_
#define _CORE__KERNEL__VM_H_

namespace Genode { class Vm_state; }

/* core includes */
#include <kernel/cpu_context.h>
#include <kernel/pd.h>
#include <kernel/signal_receiver.h>

#include <board.h>

namespace Kernel {

	/**
	 * Kernel backend for a virtual machine
	 */
	class Vm;
}


class Kernel::Vm : private Kernel::Object, public Cpu_job
{
	public:

		struct Identity
		{
			unsigned const id;
			void *   const table;
		};

	private:

		using State = Board::Vm_state;

		/*
		 * Noncopyable
		 */
		Vm(Vm const &);
		Vm &operator = (Vm const &);

		enum Scheduler_state { ACTIVE, INACTIVE };

		Irq::Pool                 & _user_irq_pool;
		Object                      _kernel_object { *this };
		State                     & _state;
		Signal_context            & _context;
		Identity const              _id;
		Scheduler_state             _scheduled = INACTIVE;
		Board::Vcpu_context         _vcpu_context;

	public:

		/**
		 * Constructor
		 *
		 * \param cpu      cpu affinity
		 * \param state    initial CPU state
		 * \param context  signal for VM exceptions other than interrupts
		 */
		Vm(Irq::Pool              & user_irq_pool,
		   Cpu                    & cpu,
		   Genode::Vm_state       & state,
		   Kernel::Signal_context & context,
		   Identity               & id);

		~Vm();

		/**
		 * Inject an interrupt to this VM
		 *
		 * \param irq  interrupt number to inject
		 */
		void inject_irq(unsigned irq);


		/**
		 * Create a virtual machine that is stopped initially
		 *
		 * \param dst                memory donation for the VM object
		 * \param state              location of the CPU state of the VM
		 * \param signal_context_id  kernel name of the signal context for VM events
		 * \param id                 VM identity
		 *
		 * \retval cap id when successful, otherwise invalid cap id
		 */
		static capid_t syscall_create(Genode::Kernel_object<Vm> & vm,
		                              unsigned                    cpu,
		                              void * const                state,
		                              capid_t const               signal_context_id,
		                              Identity                  & id)
		{
			return (capid_t)call(call_id_new_vm(), (Call_arg)&vm, (Call_arg)cpu,
			                     (Call_arg)state, (Call_arg)&id, signal_context_id);
		}

		/**
		 * Destruct a virtual-machine
		 *
		 * \param vm  pointer to vm kernel object
		 *
		 * \retval 0 when successful, otherwise !=0
		 */
		static void syscall_destroy(Genode::Kernel_object<Vm> & vm) {
			call(call_id_delete_vm(), (Call_arg) &vm); }

		Object &kernel_object() { return _kernel_object; }

		/****************
		 ** Vm_session **
		 ****************/

		void run()
		{
			if (_scheduled != ACTIVE) Cpu_job::_activate_own_share();
			_scheduled = ACTIVE;
		}

		void pause()
		{
			if (_scheduled != INACTIVE)
				Cpu_job::_deactivate_own_share();

			_scheduled = INACTIVE;
		}


		/*************
		 ** Cpu_job **
		 *************/

		void exception(Cpu & cpu) override;
		void proceed(Cpu &  cpu)  override;
		Cpu_job * helping_sink()  override { return this; }
};

#endif /* _CORE__KERNEL__VM_H_ */
