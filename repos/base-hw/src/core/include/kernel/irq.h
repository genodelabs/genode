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
	public:

		using Pool = Object_pool<Irq>;

	protected:

		Pool &_pool;

		/**
		 * Get kernel name of the interrupt
		 */
		unsigned _id() const { return Pool::Item::id(); };

	public:

		/**
		 * Constructor
		 *
		 * \param irq_id  kernel name of the interrupt
		 * \param pool    pool this interrupt shall belong to
		 */
		Irq(unsigned const irq_id, Pool &pool)
		: Pool::Item(irq_id), _pool(pool) { _pool.insert(this); }

		virtual ~Irq() { _pool.remove(this); }

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


class Kernel::User_irq : public Kernel::Irq
{
	private:

		Signal_context &_context;

		/**
		 * Get map that provides all user interrupts by their kernel names
		 */
		static Irq::Pool * _pool();

	public:

		/**
		 * Construct object that signals interrupt 'irq' via signal 'context'
		 */
		User_irq(unsigned const irq, Signal_context &context)
		: Irq(irq, *_pool()), _context(context) { disable(); }

		/**
		 * Destructor
		 */
		~User_irq() { disable(); }

		/**
		 * Handle occurence of the interrupt
		 */
		void occurred()
		{
			_context.submit(1);
			disable();
		}

		/**
		 * Handle occurence of interrupt 'irq'
		 */
		static User_irq * object(unsigned const irq) {
			return dynamic_cast<User_irq*>(_pool()->object(irq)); }
};

#endif /* _KERNEL__IRQ_H_ */
