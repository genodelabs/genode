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

	typedef Object_pool<Vm> Vm_pool;

	Vm_pool * vm_pool();
}


class Kernel::Vm : public Object<Vm, vm_pool>, public Cpu_job
{
	private:

		enum State { ACTIVE, INACTIVE };

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

		/**
		 * Inject an interrupt to this VM
		 *
		 * \param irq  interrupt number to inject
		 */
		void inject_irq(unsigned irq);


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

#endif /* _KERNEL__VM_H_ */
