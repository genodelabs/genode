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

#ifndef _CORE__INCLUDE__KERNEL__VM_H_
#define _CORE__INCLUDE__KERNEL__VM_H_

#include <vm_state.h>

/* core includes */
#include <kernel/kernel.h>
#include <kernel/pd.h>
#include <kernel/signal_receiver.h>

namespace Kernel
{
	/**
	 * Kernel backend for a virtual machine
	 */
	class Vm;
}


class Kernel::Vm : public Cpu_job,
                   public Kernel::Object
{
	private:

		enum State { ACTIVE, INACTIVE };

		unsigned                 _id;
		Genode::Vm_state * const _state;
		Signal_context   * const _context;
		void             * const _table;
		State                    _scheduled = INACTIVE;

	public:

		/**
		 * Constructor
		 *
		 * \param state    initial CPU state
		 * \param context  signal for VM exceptions other than interrupts
		 * \param table    translation table for guest to host physical memory
		 */
		Vm(void           * const state,
		   Signal_context * const context,
		   void           * const table);

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
		 * \param table              guest-physical to host-physical translation
		 *                           table pointer
		 *
		 * \retval cap id when successful, otherwise invalid cap id
		 */
		static capid_t syscall_create(void * const dst, void * const state,
		                              capid_t const signal_context_id,
		                              void * const table)
		{
			return call(call_id_new_vm(), (Call_arg)dst, (Call_arg)state,
			            (Call_arg)table, signal_context_id);
		}

		/**
		 * Destruct a virtual-machine
		 *
		 * \param vm  pointer to vm kernel object
		 *
		 * \retval 0 when successful, otherwise !=0
		 */
		static void syscall_destroy(Vm * const vm) {
			call(call_id_delete_vm(), (Call_arg) vm); }


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
			if (_scheduled != INACTIVE) Cpu_job::_deactivate_own_share();
			_scheduled = INACTIVE;
		}


		/*************
		 ** Cpu_job **
		 *************/

		void exception(unsigned const cpu);
		void proceed(unsigned const cpu);
		Cpu_job * helping_sink() { return this; }
};

#endif /* _CORE__INCLUDE__KERNEL__VM_H_ */
