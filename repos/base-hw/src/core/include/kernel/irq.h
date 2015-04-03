/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__IRQ_H_
#define _KERNEL__IRQ_H_

/* Genode includes */
#include <base/native_types.h>
#include <irq_session/irq_session.h>
#include <unmanaged_singleton.h>

/* core includes */
#include <kernel/signal_receiver.h>

namespace Kernel
{
	/**
	 * Kernel back-end interface of an interrupt
	 */
	class Irq;

	/**
	 * Kernel back-end of a user interrupt
	 */
	class User_irq;
}

namespace Genode
{
	/**
	 * Core front-end of a user interrupt
	 */
	class Irq;
}


class Kernel::Irq : public Object_pool<Irq>::Item
{
	protected:

		/**
		 * Get kernel name of the interrupt
		 */
		unsigned _id() const { return Pool::Item::id(); };

	public:

		using Pool = Object_pool<Irq>;

		/**
		 * Constructor
		 *
		 * \param irq_id  kernel name of the interrupt
		 */
		Irq(unsigned const irq_id)
		: Pool::Item(irq_id) { }

		/**
		 * Constructor
		 *
		 * \param irq_id  kernel name of the interrupt
		 * \param pool    pool this interrupt shall belong to
		 */
		Irq(unsigned const irq_id, Pool &pool)
		: Irq(irq_id) { pool.insert(this); }

		/**
		 * Destructor
		 *
		 * By now, there is no use case to destruct interrupts
		 */
		virtual ~Irq() { PERR("destruction of interrupts not implemented"); }

		/**
		 * Handle occurence of the interrupt
		 */
		virtual void occurred() { }

		/**
		 * Prevent interrupt from occurring
		 */
		void disable() const;

		/**
		 * Allow interrupt to occur
		 */
		void enable() const;
};


class Kernel::User_irq
:
	public Kernel::Irq,
	public Signal_receiver,
	public Signal_context,
	public Signal_ack_handler
{
	private:

		/**
		 * Get map that provides all user interrupts by their kernel names
		 */
		static Irq::Pool * _pool();


		/************************
		 ** Signal_ack_handler **
		 ************************/

		void _signal_acknowledged() { enable(); }

	public:

		/**
		 * Constructor
		 *
		 * \param irq_id  kernel name of the interrupt
		 */
		User_irq(unsigned const irq_id)
		: Irq(irq_id), Signal_context(this, 0)
		{
			_pool()->insert(this);
			disable();
			Signal_context::ack_handler(this);
		}

		/**
		 * Get kernel name of the interrupt-signal receiver
		 */
		unsigned receiver_id() const { return Signal_receiver::Object::id(); }

		/**
		 * Get kernel name of the interrupt-signal context
		 */
		unsigned context_id() const { return Signal_context::Object::id(); }

		/**
		 * Handle occurence of the interrupt
		 */
		void occurred()
		{
			Signal_context::submit(1);
			disable();
		}

		/**
		 * Handle occurence of an interrupt
		 *
		 * \param irq_id  kernel name of targeted interrupt
		 */
		static User_irq * object(unsigned const irq_id) {
			return dynamic_cast<User_irq*>(_pool()->object(irq_id)); }
};

#endif /* _KERNEL__IRQ_H_ */
