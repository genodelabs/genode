/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
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

/* base-hw includes */
#include <placement_new.h>

/* core includes */
#include <kernel/signal_receiver.h>

namespace Kernel
{
	/**
	 * Kernel back-end of a user interrupt
	 */
	class Irq;
}

namespace Genode
{
	/**
	 * Core front-end of a user interrupt
	 */
	class Irq;
}

class Kernel::Irq
:
	public Object_pool<Irq>::Item,
	public Signal_receiver,
	public Signal_context,
	public Signal_ack_handler
{
	friend class Genode::Irq;

	private:

		typedef Object_pool<Irq> Pool;

		/**
		 * Get map that provides all user interrupts by their kernel names
		 */
		static Pool *  _pool()
		{
			static Pool p;
			return &p;
		}

		/**
		 * Prevent interrupt from occurring
		 */
		void _disable() const;

		/**
		 * Allow interrupt to occur
		 */
		void _enable() const;

		/**
		 * Get kernel name of the interrupt
		 */
		unsigned _id() const { return Pool::Item::id(); };

		/**
		 * Get kernel name of the interrupt-signal receiver
		 */
		unsigned _receiver_id() const { return Signal_receiver::Object::id(); }

		/**
		 * Get kernel name of the interrupt-signal context
		 */
		unsigned _context_id() const { return Signal_context::Object::id(); }

		/**
		 * Handle occurence of the interrupt
		 */
		void _occurred()
		{
			Signal_context::submit(1);
			_disable();
		}

		/**
		 * Get information that enables a user to handle an interrupt
		 *
		 * \param irq_id  kernel name of targeted interrupt
		 */
		static Genode::Irq_signal _genode_signal(unsigned const irq_id)
		{
			typedef Genode::Irq_signal Irq_signal;
			static Irq_signal const invalid = { 0, 0 };
			Irq * const irq = _pool()->object(irq_id);
			if (irq) {
				Irq_signal s = { irq->_receiver_id(), irq->_context_id() };
				return s;
			}
			return invalid;
		}

		/**
		 * Destructor
		 *
		 * By now, there is no use case to destruct user interrupts
		 */
		~Irq()
		{
			PERR("destruction of interrupts not implemented");
			while (1) { }
		}


		/************************
		 ** Signal_ack_handler **
		 ************************/

		void _signal_acknowledged() { _enable(); }

	public:

		/**
		 * Constructor
		 *
		 * \param irq_id  kernel name of the interrupt
		 */
		Irq(unsigned const irq_id)
		:
			Pool::Item(irq_id),
			Signal_context(this, 0)
		{
			Signal_context::ack_handler(this);
			_pool()->insert(this);
			_disable();
		}

		/**
		 * Handle occurence of an interrupt
		 *
		 * \param irq_id  kernel name of targeted interrupt
		 */
		static void occurred(unsigned const irq_id)
		{
			Irq * const irq = _pool()->object(irq_id);
			if (!irq) {
				PWRN("unknown interrupt occurred");
				return;
			}
			irq->_occurred();
		}
};

class Genode::Irq
{
	public:

		/**
		 * Get information that enables a user to handle an interrupt
		 *
		 * \param irq_id  kernel name of targeted interrupt
		 */
		static Irq_signal signal(unsigned const irq_id)
		{
			return Kernel::Irq::_genode_signal(irq_id);
		}
};

#endif /* _KERNEL__IRQ_H_ */
