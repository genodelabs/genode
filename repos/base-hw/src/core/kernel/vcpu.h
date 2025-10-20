/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \author  Benjamin Lamowski
 * \author  Stefan Kalkowski
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__VCPU_H_
#define _CORE__KERNEL__VCPU_H_

/* core includes */
#include <kernel/cpu_context.h>
#include <kernel/pd.h>
#include <kernel/signal.h>

#include <board.h>

namespace Kernel {

	class Cpu;
	class Thread;

	/**
	 * Kernel backend for a virtual machine
	 */
	class Vcpu;
}


class Kernel::Vcpu : private Kernel::Object, public Cpu_context
{
	public:

		struct Identity
		{
			using Id = Genode::Attempt<addr_t, Genode::Bit_array_base::Error>;
			Id     const id;
			void * const table;
		};

	private:

		using Vcpu_state = Board::Vcpu_state;

		/*
		 * Noncopyable
		 */
		Vcpu(Vcpu const &);
		Vcpu &operator = (Vcpu const &);

		enum Scheduler_state { ACTIVE, INACTIVE };

		Irq::Pool          &_user_irq_pool;
		Object              _kernel_object { *this };
		Vcpu_state         &_state;
		Signal_context     &_context;
		Identity const      _id;
		Scheduler_state     _scheduled = INACTIVE;
		Board::Vcpu_context _vcpu_context;

		void _pause_vcpu()
		{
			if (_scheduled != INACTIVE)
				Cpu_context::_deactivate();

			_scheduled = INACTIVE;
		}

		friend class Thread;

	public:

		/**
		 * Constructor
		 *
		 * \param cpu      cpu affinity
		 * \param state    initial CPU state
		 * \param context  signal for VM exceptions other than interrupts
		 */
		Vcpu(Irq::Pool              &user_irq_pool,
		     Cpu                    &cpu,
		     Vcpu_state             &state,
		     Kernel::Signal_context &context,
		     Identity               &id);

		~Vcpu();

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
		 * \param data               location of the CPU data of the VM
		 * \param signal_context_id  kernel name of the signal context for VM events
		 * \param id                 VM identity
		 *
		 * \retval cap id when successful, otherwise invalid cap id
		 */
		static capid_t syscall_create(Core::Kernel_object<Vcpu> &vcpu,
		                              unsigned                   cpu,
		                              void * const               data,
		                              capid_t const              signal_context_id,
		                              Identity                  &id)
		{
			return (capid_t)core_call(Core_call_id::VCPU_CREATE,
			                          (Call_arg)&vcpu, (Call_arg)cpu,
			                          (Call_arg)data, (Call_arg)&id,
			                          signal_context_id);
		}

		/**
		 * Destruct a virtual-machine
		 *
		 * \param vcpu  pointer to vcpu kernel object
		 *
		 * \retval 0 when successful, otherwise !=0
		 */
		static void syscall_destroy(Core::Kernel_object<Vcpu> &vcpu) {
			core_call(Core_call_id::VCPU_DESTROY, (Call_arg) &vcpu); }

		Object &kernel_object() { return _kernel_object; }


		/******************
		 ** Vcpu_session **
		 ******************/

		void run();
		void pause();


		/*****************
		 ** Cpu_context **
		 *****************/

		void exception(Genode::Cpu_state&) override;
		void proceed() override;
};

#endif /* _CORE__KERNEL__VCPU_H_ */
