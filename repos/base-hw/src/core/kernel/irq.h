/*
 * \brief   Kernel abstractions of interrupts
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__IRQ_H_
#define _CORE__KERNEL__IRQ_H_

/* Genode includes */
#include <irq_session/irq_session.h>
#include <util/avl_tree.h>

/* core includes */
#include <kernel/signal.h>

namespace Board { class Local_interrupt_controller; }


namespace Kernel {

	/**
	 * Kernel back-end interface of an interrupt
	 */
	class Irq;

	/**
	 * Kernel back-end of a user interrupt
	 */
	class User_irq;
}


namespace Core {

	/**
	 * Core front-end of a user interrupt
	 */
	class Irq;
}


class Kernel::Irq : Genode::Avl_node<Irq>
{
	public:

		class Pool;

	protected:

		friend class Genode::Avl_tree<Irq>;
		friend class Genode::Avl_node<Irq>;

		unsigned _id;
		Pool    &_pool;

		Board::Local_interrupt_controller &_pic;

		void _with(unsigned const id,
		           auto const &found_fn,
		           auto const &missedfn)
		{
			if (id == _id) {
				found_fn(*this);
				return;
			}

			Irq * const subtree = Genode::Avl_node<Irq>::child(id > _id);
			if (!subtree) missedfn();
			else subtree->_with(id, found_fn, missedfn);
		}


		/************************
		 * 'Avl_node' interface *
		 ************************/

		bool higher(Irq *i) const { return i->_id > _id; }

	public:

		using Controller = Board::Local_interrupt_controller;

		Irq(unsigned const irq, Pool &irq_pool, Controller &pic);

		virtual ~Irq();

		virtual void occurred() { }

		void enable() const;
		void disable() const;

		unsigned id() { return _id; }
};


class Kernel::Irq::Pool
{
	private:

		friend class Irq;

		Genode::Avl_tree<Irq> _tree {};

	public:

		void with(unsigned const id,
		          auto const &found_fn,
		          auto const &missedfn) const
		{
			if (!_tree.first()) missedfn();
			else _tree.first()->_with(id, found_fn, missedfn);
		}
};


class Kernel::User_irq : public Kernel::Irq
{
	private:

		Kernel::Object  _kernel_object { *this };
		Signal_context &_context;

	public:

		using Trigger    = Genode::Irq_session::Trigger;
		using Polarity   = Genode::Irq_session::Polarity;

		User_irq(unsigned const id, Trigger trigger, Polarity polarity,
		         Signal_context &context, Controller &pic, Pool &pool);

		~User_irq();

		void occurred() override;

		Object &kernel_object() { return _kernel_object; }

		/**
		 * Syscall to create user irq object
		 *
		 * \param irq       reference to constructible object
		 * \param nr        interrupt number
		 * \param trigger   level or edge
		 * \param polarity  low or high
		 * \param sig       capability of signal context
		 */
		static capid_t syscall_create(Core::Kernel_object<User_irq> &irq,
		                              unsigned                       nr,
		                              Genode::Irq_session::Trigger   trigger,
		                              Genode::Irq_session::Polarity  polarity,
		                              capid_t                        sig)
		{
			return (capid_t)core_call(Core_call_id::IRQ_CREATE, (Call_arg)&irq,
			                          nr, trigger, polarity, sig);
		}

		/**
		 * Syscall to delete user irq object
		 *
		 * \param irq  reference to constructible object
		 */
		static void syscall_destroy(Core::Kernel_object<User_irq> &irq) {
			core_call(Core_call_id::IRQ_DESTROY, (Call_arg) &irq); }
};

#endif /* _CORE__KERNEL__IRQ_H_ */
