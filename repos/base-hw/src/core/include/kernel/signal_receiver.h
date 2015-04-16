/*
 * \brief   Kernel backend for asynchronous inter-process communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__SIGNAL_RECEIVER_H_
#define _KERNEL__SIGNAL_RECEIVER_H_

/* Genode includes */
#include <base/signal.h>

/* core include */
#include <kernel/object.h>

namespace Kernel
{
	/**
	 * Ability to receive signals from signal receivers
	 */
	class Signal_handler;

	/**
	 * Ability to get informed about signal acks
	 */
	class Signal_ack_handler;

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

	typedef Object_pool<Signal_context>  Signal_context_pool;
	typedef Object_pool<Signal_receiver> Signal_receiver_pool;

	Signal_context_pool  * signal_context_pool();
	Signal_receiver_pool * signal_receiver_pool();
}

class Kernel::Signal_ack_handler
{
	friend class Signal_context;

	private:

		Signal_context * _signal_context;

	protected:

		/**
		 * Provide custom handler for acks at a signal context
		 */
		virtual void _signal_acknowledged() = 0;

		/**
		 * Constructor
		 */
		Signal_ack_handler() : _signal_context(0) { }

		/**
		 * Destructor
		 */
		virtual ~Signal_ack_handler();
};

class Kernel::Signal_handler
{
	friend class Signal_receiver;

	private:

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

class Kernel::Signal_context
: public Object<Signal_context, signal_context_pool>
{
	friend class Signal_receiver;
	friend class Signal_context_killer;

	private:

		typedef Genode::Fifo_element<Signal_context> Fifo_element;

		/**
		 * Dummy handler that is used every time no other handler is available
		 */
		class Default_ack_handler : public Signal_ack_handler
		{
			private:

				/************************
				 ** Signal_ack_handler **
				 ************************/

				void _signal_acknowledged() { }
		};

		Fifo_element            _deliver_fe;
		Fifo_element            _contexts_fe;
		Signal_receiver * const _receiver;
		unsigned const          _imprint;
		unsigned                _submits;
		bool                    _ack;
		bool                    _killed;
		Signal_context_killer * _killer;
		Default_ack_handler     _default_ack_handler;
		Signal_ack_handler    * _ack_handler;

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
		Signal_context(Signal_receiver * const r, unsigned const imprint);

		/**
		 * Attach or detach a handler for acknowledgments at this context
		 *
		 * \param h  handler that shall be attached or 0 to detach handler
		 */
		void ack_handler(Signal_ack_handler * const h);

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
};

class Kernel::Signal_receiver
: public Object<Signal_receiver, signal_receiver_pool>
{
	friend class Signal_context;
	friend class Signal_handler;

	private:

		typedef Genode::Signal Signal;

		template <typename T> class Fifo : public Genode::Fifo<T> { };

		Fifo<Signal_handler::Fifo_element> _handlers;
		Fifo<Signal_context::Fifo_element> _deliver;
		Fifo<Signal_context::Fifo_element> _contexts;

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
		 * Return wether any of the contexts of this receiver is deliverable
		 */
		bool deliverable();
};

#endif /* _KERNEL__SIGNAL_RECEIVER_ */
