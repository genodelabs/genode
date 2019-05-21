/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__IRQ_H_
#define _CORE__KERNEL__IRQ_H_

/* Genode includes */
#include <irq_session/irq_session.h>
#include <util/avl_tree.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

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


class Kernel::Irq : Genode::Avl_node<Irq>
{
	friend class Genode::Avl_tree<Irq>;
	friend class Genode::Avl_node<Irq>;

	public:

		struct Pool : Genode::Avl_tree<Irq>
		{
			Irq * object(unsigned const id) const
			{
				Irq * const irq = first();
				if (!irq) return nullptr;
				return irq->find(id);
			}
		};

	protected:

		unsigned _irq_nr; /* kernel name of the interrupt */
		Pool    &_pool;

	public:

		/**
		 * Constructor
		 *
		 * \param irq   interrupt number
		 * \param pool  pool this interrupt shall belong to
		 */
		Irq(unsigned const irq, Pool &pool)
		: _irq_nr(irq), _pool(pool) { _pool.insert(this); }

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

		unsigned irq_number() { return _irq_nr; }


		/************************
		 * 'Avl_node' interface *
		 ************************/

		bool higher(Irq * i) const { return i->_irq_nr > _irq_nr; }

		/**
		 * Find irq with 'nr' within this AVL subtree
		 */
		Irq * find(unsigned const nr)
		{
			if (nr == _irq_nr) return this;
			Irq * const subtree = Genode::Avl_node<Irq>::child(nr > _irq_nr);
			return (subtree) ? subtree->find(nr): nullptr;
		}

};


class Kernel::User_irq : public Kernel::Irq
{
	private:

		Kernel::Object  _kernel_object { *this };
		Signal_context &_context;

		/**
		 * Get map that provides all user interrupts by their kernel names
		 */
		static Irq::Pool &_pool();

	public:

		/**
		 * Construct object that signals interrupt 'irq' via signal 'context'
		 */
		User_irq(unsigned const                irq,
		         Genode::Irq_session::Trigger  trigger,
		         Genode::Irq_session::Polarity polarity,
		         Signal_context              & context);

		/**
		 * Destructor
		 */
		~User_irq() { disable(); }

		/**
		 * Handle occurence of the interrupt
		 */
		void occurred() override
		{
			if (_context.can_submit(1)) {
				_context.submit(1);
			}
			disable();
		}

		/**
		 * Handle occurence of interrupt 'irq'
		 */
		static User_irq * object(unsigned const irq) {
			return dynamic_cast<User_irq*>(_pool().object(irq)); }

		/**
		 * Syscall to create user irq object
		 *
		 * \param irq       reference to constructible object
		 * \param nr        interrupt number
		 * \param trigger   level or edge
		 * \param polarity  low or high
		 * \param sig       capability of signal context
		 */
		static capid_t syscall_create(Genode::Kernel_object<User_irq> & irq,
		                              unsigned                          nr,
		                              Genode::Irq_session::Trigger      trigger,
		                              Genode::Irq_session::Polarity     polarity,
		                              capid_t                           sig)
		{
			return call(call_id_new_irq(), (Call_arg)&irq, nr,
			            (trigger << 2) | polarity, sig);
		}

		/**
		 * Syscall to delete user irq object
		 *
		 * \param irq  reference to constructible object
		 */
		static void syscall_destroy(Genode::Kernel_object<User_irq> &irq) {
			call(call_id_delete_irq(), (Call_arg) &irq); }

		Object &kernel_object() { return _kernel_object; }
};

#endif /* _CORE__KERNEL__IRQ_H_ */
