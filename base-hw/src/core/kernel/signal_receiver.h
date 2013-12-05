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
#include <util/fifo.h>
#include <base/signal.h>

/* core include */
#include <kernel/configuration.h>
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
	 * Ability to destruct signal receivers
	 */
	class Signal_receiver_killer;

	/**
	 * Signal types that are assigned to a signal receiver each
	 */
	class Signal_context;

	/**
	 * Combines signal contexts to an entity that handlers can listen to
	 */
	class Signal_receiver;

	class Signal_context_ids  : public Id_allocator<MAX_SIGNAL_CONTEXTS> { };
	class Signal_receiver_ids : public Id_allocator<MAX_SIGNAL_RECEIVERS> { };
	typedef Object_pool<Signal_context>  Signal_context_pool;
	typedef Object_pool<Signal_receiver> Signal_receiver_pool;

	Signal_context_ids   * signal_context_ids();
	Signal_context_pool  * signal_context_pool();
	Signal_receiver_ids  * signal_receiver_ids();
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
		 * Backend for for destructor and cancel_waiting
		 */
		void _cancel_waiting();

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

		/**
		 * Constructor
		 */
		Signal_handler()
		:
			_handlers_fe(this),
			_receiver(0)
		{ }

		/**
		 * Destructor
		 */
		virtual ~Signal_handler() { _cancel_waiting(); }

		/**
		 * Stop waiting for a signal receiver
		 */
		void cancel_waiting() { _cancel_waiting(); }
};

class Kernel::Signal_context_killer
{
	friend class Signal_context;

	private:

		Signal_context * _context;

		/**
		 * Backend for destructor and cancel_waiting
		 */
		void _cancel_waiting();

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

		/**
		 * Constructor
		 */
		Signal_context_killer() : _context(0) { }

		/**
		 * Destructor
		 */
		virtual ~Signal_context_killer() { _cancel_waiting(); }

		/**
		 * Stop waiting for a signal context
		 */
		void cancel_waiting() { _cancel_waiting(); }
};

class Kernel::Signal_receiver_killer
{
	friend class Signal_receiver;

	private:

		Signal_receiver * _receiver;

		/**
		 * Backend for for destructor and cancel_waiting
		 */
		void _cancel_waiting();

		/**
		 * Notice that the destruction is pending
		 */
		virtual void _signal_receiver_kill_pending() = 0;

		/**
		 * Notice that pending destruction is done
		 */
		virtual void _signal_receiver_kill_done() = 0;

	protected:

		/***************
		 ** Accessors **
		 ***************/

		Signal_receiver * receiver() const { return _receiver; }

	public:

		/**
		 * Constructor
		 */
		Signal_receiver_killer() : _receiver(0) { }

		/**
		 * Destructor
		 */
		virtual ~Signal_receiver_killer() { _cancel_waiting(); }

		/**
		 * Stop waiting for a signal receiver
		 */
		void cancel_waiting() { _cancel_waiting(); }
};

class Kernel::Signal_context
:
	public Object<Signal_context, MAX_SIGNAL_CONTEXTS,
	              Signal_context_ids, signal_context_ids, signal_context_pool>
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
		void _delivered()
		{
			_submits = 0;
			_ack     = 0;
		}

		/**
		 * Notice that the killer of the context has cancelled waiting
		 */
		void _killer_cancelled() { _killer = 0; }

	public:

		/**
		 * Destructor
		 */
		~Signal_context();

		/**
		 * Exception types
		 */
		struct Assign_to_receiver_failed { };

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
		void ack_handler(Signal_ack_handler * const h)
		{
			_ack_handler = h ? h : &_default_ack_handler;
			_ack_handler->_signal_context = this;
		}

		/**
		 * Submit the signal
		 *
		 * \param n  number of submits
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int submit(unsigned const n)
		{
			if (_killed || _submits >= (unsigned)~0 - n) { return -1; }
			_submits += n;
			if (_ack) { _deliverable(); }
			return 0;
		}

		/**
		 * Acknowledge delivery of signal
		 */
		void ack()
		{
			_ack_handler->_signal_acknowledged();
			if (_ack) { return; }
			if (!_killed) {
				_ack = 1;
				_deliverable();
				return;
			}
			if (_killer) {
				_killer->_context = 0;
				_killer->_signal_context_kill_done();
				_killer = 0;
			}
		}

		/**
		 * Destruct context or prepare to do it as soon as delivery is done
		 *
		 * \param killer  object that shall receive progress reports
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int kill(Signal_context_killer * const k)
		{
			/* check if in a kill operation or already killed */
			if (_killed) {
				if (_ack) { return 0; }
				return -1;
			}
			/* kill directly if there is no unacknowledged delivery */
			if (_ack) {
				_killed = 1;
				return 0;
			}
			/* wait for delivery acknowledgement */
			_killer = k;
			_killed = 1;
			_killer->_context = this;
			_killer->_signal_context_kill_pending();
			return 0;
		}
};

class Kernel::Signal_receiver
:
	public Object<Signal_receiver, MAX_SIGNAL_RECEIVERS,
	              Signal_receiver_ids, signal_receiver_ids,
	              signal_receiver_pool>,
	public Signal_context_killer
{
	friend class Signal_context;
	friend class Signal_handler;
	friend class Signal_receiver_killer;

	private:

		typedef Genode::Signal Signal;

		template <typename T> class Fifo : public Genode::Fifo<T> { };

		Fifo<Signal_handler::Fifo_element> _handlers;
		Fifo<Signal_context::Fifo_element> _deliver;
		Fifo<Signal_context::Fifo_element> _contexts;
		bool                               _kill;
		Signal_receiver_killer *           _killer;
		unsigned                           _context_kills;

		/**
		 * Recognize that context 'c' has submits to deliver
		 */
		void _add_deliverable(Signal_context * const c)
		{
			if (!c->_deliver_fe.is_enqueued()) {
				_deliver.enqueue(&c->_deliver_fe);
			}
			_listen();
		}

		/**
		 * Deliver as much submits as possible
		 */
		void _listen()
		{
			while (1)
			{
				/* check if there are deliverable signal */
				if (_deliver.empty()) return;
				Signal_context * const c = _deliver.dequeue()->object();

				/* if there is no handler re-enqueue context and exit */
				if (_handlers.empty()) {
					_deliver.enqueue(&c->_deliver_fe);
					return;
				}
				/* delivery from context to handler */
				Signal_handler * const h = _handlers.dequeue()->object();
				Signal::Data data((Genode::Signal_context *)c->_imprint,
				                   c->_submits);
				h->_receiver = 0;
				h->_receive_signal(&data, sizeof(data));
				c->_delivered();
			}
		}

		/**
		 * Notice that a context of the receiver has been destructed
		 *
		 * \param c  killed context
		 */
		void _context_destructed(Signal_context * const c)
		{
			_contexts.remove(&c->_contexts_fe);
			if (!c->_deliver_fe.is_enqueued()) { return; }
			_deliver.remove(&c->_deliver_fe);
		}

		/**
		 * Notice that the killer of the receiver has cancelled waiting
		 */
		void _killer_cancelled() { _killer = 0; }

		/**
		 * Notice that handler 'h' has cancelled waiting
		 */
		void _handler_cancelled(Signal_handler * const h)
		{
			_handlers.remove(&h->_handlers_fe);
		}

		/**
		 * Assign context 'c' to the receiver
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int _add_context(Signal_context * const c)
		{
			if (_kill) { return -1; }
			_contexts.enqueue(&c->_contexts_fe);
			return 0;
		}


		/***************************
		 ** Signal_context_killer **
		 ***************************/

		void _signal_context_kill_pending() { _context_kills++; }

		void _signal_context_kill_done()
		{
			_context_kills--;
			if (!_context_kills && _kill) {
				this->~Signal_receiver();
				if (_killer) {
					_killer->_receiver = 0;
					_killer->_signal_receiver_kill_done();
				}
			}
		}

		void _signal_context_kill_failed() { PERR("unexpected call"); }

	public:

		/**
		 * Constructor
		 */
		Signal_receiver()
		:
			_kill(0),
			_killer(0),
			_context_kills(0)
		{ }

		/**
		 * Let a handler 'h' wait for signals of the receiver
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int add_handler(Signal_handler * const h)
		{
			if (_kill || h->_receiver) { return -1; }
			_handlers.enqueue(&h->_handlers_fe);
			h->_receiver = this;
			h->_await_signal(this);
			_listen();
			return 0;
		}

		/**
		 * Return wether any of the contexts of this receiver is deliverable
		 */
		bool deliverable() { return !_deliver.empty(); }

		/**
		 * Destruct receiver or prepare to do it as soon as delivery is done
		 *
		 * \param killer  object that shall receive progress reports
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int kill(Signal_receiver_killer * const k)
		{
			if (_kill) { return -1; }

			/* start killing at all contexts of the receiver */
			Signal_context * c = _contexts.dequeue()->object();
			while (c) {
				c->kill(this);
				c->~Signal_context();
				c = _contexts.dequeue()->object();
			}
			/* destruct directly if no context kill is pending */
			if (!_context_kills) {
				this->~Signal_receiver();
				return 0;
			}
			/* wait for pending context kills */
			_kill              = 1;
			_killer            = k;
			_killer->_receiver = this;
			_killer->_signal_receiver_kill_pending();
			return 0;
		}
};

#endif /* _KERNEL__SIGNAL_RECEIVER_ */
