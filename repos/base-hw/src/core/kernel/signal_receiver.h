/*
 * \brief   Kernel backend for asynchronous inter-process communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__SIGNAL_RECEIVER_H_
#define _CORE__KERNEL__SIGNAL_RECEIVER_H_

/* Genode includes */
#include <base/signal.h>

#include <kernel/core_interface.h>
#include <kernel/object.h>

namespace Kernel
{
	/**
	 * Ability to receive signals from signal receivers
	 */
	class Signal_handler;

	/**
	 * Ability to destruct signal contexts
	 */
	class Signal_context_killer;

	/**
	 * Signal types that are assigned to a signal receiver each
	 */
	class Signal_context;

	/**
	 * Combines signal contexts to an entity that handlers can listen to
	 */
	class Signal_receiver;
}


class Kernel::Signal_handler
{
	friend class Signal_receiver;

	private:

		/*
		 * Noncopyable
		 */
		Signal_handler(Signal_handler const &);
		Signal_handler &operator = (Signal_handler const &);

		typedef Genode::Fifo_element<Signal_handler> Fifo_element;

		Fifo_element      _handlers_fe;
		Signal_receiver * _receiver;

		/**
		 * Let the handler block for signal receipt
		 *
		 * \param receiver  the signal pool that the thread blocks for
		 */
		virtual void _await_signal(Signal_receiver * const receiver) = 0;

		/**
		 * Signal delivery backend
		 *
		 * \param base  signal-data base
		 * \param size  signal-data size
		 */
		virtual void _receive_signal(void * const base, size_t const size) = 0;

	protected:

		/***************
		 ** Accessors **
		 ***************/

		Signal_receiver * receiver() const { return _receiver; }

	public:

		Signal_handler();
		virtual ~Signal_handler();

		/**
		 * Stop waiting for a signal receiver
		 */
		void cancel_waiting();
};

class Kernel::Signal_context_killer
{
	friend class Signal_context;

	private:

		/*
		 * Noncopyable
		 */
		Signal_context_killer(Signal_context_killer const &);
		Signal_context_killer &operator = (Signal_context_killer const &);

		Signal_context * _context;

		/**
		 * Notice that the kill operation is pending
		 */
		virtual void _signal_context_kill_pending() = 0;

		/**
		 * Notice that pending kill operation is done
		 */
		virtual void _signal_context_kill_done() = 0;

		/**
		 * Notice that pending kill operation failed
		 */
		virtual void _signal_context_kill_failed() = 0;

	protected:

		/***************
		 ** Accessors **
		 ***************/

		Signal_context * context() const { return _context; }

	public:

		Signal_context_killer();
		virtual ~Signal_context_killer();

		/**
		 * Stop waiting for a signal context
		 */
		void cancel_waiting();
};

class Kernel::Signal_context : public Kernel::Object
{
	friend class Signal_receiver;
	friend class Signal_context_killer;

	private:

		/*
		 * Noncopyable
		 */
		Signal_context(Signal_context const &);
		Signal_context &operator = (Signal_context const &);

		typedef Genode::Fifo_element<Signal_context> Fifo_element;

		Fifo_element            _deliver_fe;
		Fifo_element            _contexts_fe;
		Signal_receiver * const _receiver;
		addr_t const            _imprint;
		unsigned                _submits;
		bool                    _ack;
		bool                    _killed;
		Signal_context_killer * _killer;

		/**
		 * Tell receiver about the submits of the context if any
		 */
		void _deliverable();

		/**
		 * Called by receiver when all submits have been delivered
		 */
		void _delivered();

		/**
		 * Notice that the killer of the context has cancelled waiting
		 */
		void _killer_cancelled();

	public:

		/**
		 * Destructor
		 */
		~Signal_context();

		/**
		 * Constructor
		 *
		 * \param r        receiver that the context shall be assigned to
		 * \param imprint  userland identification of the context
		 *
		 * \throw  Assign_to_receiver_failed
		 */
		Signal_context(Signal_receiver * const r, addr_t const imprint);

		/**
		 * Submit the signal
		 *
		 * \param n  number of submits
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int submit(unsigned const n);

		/**
		 * Acknowledge delivery of signal
		 */
		void ack();

		/**
		 * Destruct context or prepare to do it as soon as delivery is done
		 *
		 * \param killer  object that shall receive progress reports
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int kill(Signal_context_killer * const k);

		/**
		 * Create a signal context and assign it to a signal receiver
		 *
		 * \param p         memory donation for the kernel signal-context object
		 * \param receiver  pointer to signal receiver kernel object
		 * \param imprint   user label of the signal context
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(void * p,
		                              Signal_receiver * const receiver,
		                              addr_t const imprint)
		{
			return call(call_id_new_signal_context(), (Call_arg)p,
			            (Call_arg)receiver, (Call_arg)imprint);
		}

		/**
		 * Destruct a signal context
		 *
		 * \param context  pointer to signal context kernel object
		 */
		static void syscall_destroy(Signal_context * const context) {
			call(call_id_delete_signal_context(), (Call_arg)context); }
};

class Kernel::Signal_receiver : public Kernel::Object
{
	friend class Signal_context;
	friend class Signal_handler;

	private:

		typedef Genode::Signal Signal;

		template <typename T> class Fifo : public Genode::Fifo<T> { };

		Fifo<Signal_handler::Fifo_element> _handlers { };
		Fifo<Signal_context::Fifo_element> _deliver  { };
		Fifo<Signal_context::Fifo_element> _contexts { };

		/**
		 * Recognize that context 'c' has submits to deliver
		 */
		void _add_deliverable(Signal_context * const c);

		/**
		 * Deliver as much submits as possible
		 */
		void _listen();

		/**
		 * Notice that a context of the receiver has been destructed
		 *
		 * \param c  destructed context
		 */
		void _context_destructed(Signal_context * const c);

		/**
		 * Notice that handler 'h' has cancelled waiting
		 */
		void _handler_cancelled(Signal_handler * const h);

		/**
		 * Assign context 'c' to the receiver
		 */
		void _add_context(Signal_context * const c);

	public:

		~Signal_receiver();

		/**
		 * Let a handler 'h' wait for signals of the receiver
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int add_handler(Signal_handler * const h);

		/**
		 * Syscall to create a signal receiver
		 *
		 * \param p  memory donation for the kernel signal-receiver object
		 *
		 * \retval capability id of the new kernel object
		 */
		static capid_t syscall_create(void * p) {
			return call(call_id_new_signal_receiver(), (Call_arg)p); }

		/**
		 * Syscall to destruct a signal receiver
		 *
		 * \param receiver  pointer to signal receiver kernel object
		 */
		static void syscall_destroy(Signal_receiver * const receiver) {
			call(call_id_delete_signal_receiver(), (Call_arg)receiver); }
};

#endif /* _CORE__KERNEL__SIGNAL_RECEIVER_H_ */
