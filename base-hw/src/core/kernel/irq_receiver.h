/*
 * \brief   Exclusive ownership and handling of interrupts
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__IRQ_RECEIVER_H_
#define _KERNEL__IRQ_RECEIVER_H_

/* core includes */
#include <pic.h>
#include <assert.h>
#include <kernel/object.h>

namespace Kernel
{
	/**
	 * Exclusive ownership and handling of one interrupt at a time
	 */
	class Irq_receiver;

	/**
	 * Return interrupt-controller singleton
	 */
	static Pic * pic() { return unsynchronized_singleton<Pic>(); }
}

class Kernel::Irq_receiver : public Object_pool<Irq_receiver>::Item
{
	private:

		typedef Object_pool<Irq_receiver> Pool;

		/**
		 * Return map that maps assigned interrupts to their receivers
		 */
		static Pool * _pool() { static Pool _pool; return &_pool; }

		/**
		 * Translate receiver ID 'id' to interrupt ID
		 */
		static unsigned _id_to_irq(unsigned id) { return id - 1; }

		/**
		 * Translate interrupt ID 'id' to receiver ID
		 */
		static unsigned _irq_to_id(unsigned irq) { return irq + 1; }

		/**
		 * Free interrupt of this receiver without sanity checks
		 */
		void _free_irq()
		{
			_pool()->remove(this);
			_id = 0;
		}

		/**
		 * Stop receiver from waiting for its interrupt without sanity checks
		 */
		void _cancel_waiting() { pic()->mask(_id_to_irq(_id)); }

		/**
		 * Gets called as soon as the receivers interrupt occurs
		 */
		virtual void _received_irq() = 0;

		/**
		 * Gets called when receiver starts waiting for its interrupt
		 */
		virtual void _awaits_irq() = 0;

	public:

		/**
		 * Constructor
		 */
		Irq_receiver() : Pool::Item(0) { }

		/**
		 * Destructor
		 */
		~Irq_receiver()
		{
			if (_id) {
				_cancel_waiting();
				_free_irq();
			}
		}

		/**
		 * Assign interrupt 'irq' to the receiver
		 *
		 * \return  wether the assignment succeeded
		 */
		bool allocate_irq(unsigned const irq)
		{
			/* check if an allocation is needed and possible */
			unsigned const id = _irq_to_id(irq);
			if (_id) { return _id == id; }
			if (_pool()->object(id)) { return 0; }

			/* allocate and mask the interrupt */
			pic()->mask(irq);
			_id = id;
			_pool()->insert(this);
			return 1;
		}

		/**
		 * Unassign interrupt 'irq' if it is assigned to the receiver
		 *
		 * \return  wether the unassignment succeeded
		 */
		bool free_irq(unsigned const irq)
		{
			if (_id != _irq_to_id(irq)) { return 0; }
			_free_irq();
			return 1;
		}

		/**
		 * Unmask and await interrupt that is assigned to the receiver
		 */
		void await_irq()
		{
			if (!_id) {
				PERR("waiting for imaginary interrupt");
				return;
			}
			unsigned const irq = _id_to_irq(_id);
			pic()->unmask(irq);
			_awaits_irq();
		}

		/**
		 * Stop waiting for the interrupt of the receiver
		 */
		void cancel_waiting() { if (_id) { _cancel_waiting(); } }

		/**
		 * Denote that the receivers interrupt 'irq' occured and mask it
		 */
		void receive_irq(unsigned const irq)
		{
			assert(_id == _irq_to_id(irq));
			pic()->mask(irq);
			_received_irq();
		}

		/**
		 * Get receiver of IRQ 'irq' or 0 if the IRQ isn't assigned
		 */
		static Irq_receiver * receiver(unsigned irq)
		{
			return _pool()->object(_irq_to_id(irq));
		}
};

#endif /* _KERNEL__IRQ_RECEIVER_H_ */
