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
	 * Ability to receive from signal receivers
	 */
	class Signal_handler;

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
}

class Kernel::Signal_handler
{
	friend class Signal_receiver;

	private:

		typedef Genode::Fifo_element<Signal_handler> Fifo_element;

		Fifo_element _handlers_fe;

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

	public:

		/**
		 * Constructor
		 */
		Signal_handler() : _handlers_fe(this) { }
};

class Kernel::Signal_context_killer
{
	friend class Signal_context;

	private:

		/**
		 * Notice that the destruction is pending
		 */
		virtual void _signal_context_kill_pending() = 0;

		/**
		 * Notice that pending destruction is done
		 */
		virtual void _signal_context_kill_done() = 0;
};

class Kernel::Signal_receiver_killer
{
	friend class Signal_receiver;

	private:

		/**
		 * Notice that the destruction is pending
		 */
		virtual void _signal_receiver_kill_pending() = 0;

		/**
		 * Notice that pending destruction is done
		 */
		virtual void _signal_receiver_kill_done() = 0;
};

class Kernel::Signal_context
:
	public Object<Signal_context, MAX_SIGNAL_CONTEXTS>
{
	friend class Signal_receiver;

	private:

		typedef Genode::Fifo_element<Signal_context> Fifo_element;

		Fifo_element            _deliver_fe;
		Fifo_element            _contexts_fe;
		Signal_receiver * const _receiver;
		unsigned const          _imprint;
		unsigned                _submits;
		bool                    _ack;
		Signal_context_killer * _killer;

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
		 * Destructor
		 */
		~Signal_context();

		/**
		 * Constructor
		 *
		 * \param r        receiver that the context is assigned to
		 * \param imprint  userland identification of the context
		 */
		Signal_context(Signal_receiver * const r, unsigned const imprint)
		:
			_deliver_fe(this), _contexts_fe(this), _receiver(r),
			_imprint(imprint), _submits(0), _ack(1), _killer(0)
		{ }

	public:

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
			if (_killer || _submits >= (unsigned)~0 - n) { return -1; }
			_submits += n;
			if (_ack) { _deliverable(); }
			return 0;
		}

		/**
		 * Acknowledge delivery of signal
		 */
		void ack()
		{
			if (_ack) { return; }
			if (!_killer) {
				_ack = 1;
				_deliverable();
				return;
			}
			this->~Signal_context();
			_killer->_signal_context_kill_done();
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
			if (_killer) { return -1; }

			/* destruct directly if there is no unacknowledged delivery */
			if (_ack) {
				this->~Signal_context();
				return 0;
			}
			/* wait for delivery acknowledgement */
			_killer = k;
			_killer->_signal_context_kill_pending();
			return 0;
		}
};

class Kernel::Signal_receiver
:
	public Object<Signal_receiver, MAX_SIGNAL_RECEIVERS>,
	public Signal_context_killer
{
	friend class Signal_context;
	friend class Context_killer;

	private:

		typedef Genode::Signal Signal;

		template <typename T> class Fifo : public Genode::Fifo<T> { };

		Fifo<Signal_handler::Fifo_element> _handlers;
		Fifo<Signal_context::Fifo_element> _deliver;
		Fifo<Signal_context::Fifo_element> _contexts;
		unsigned                           _context_kills;
		Signal_receiver_killer *           _killer;

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
				h->_receive_signal(&data, sizeof(data));
				c->_delivered();
			}
		}

		/**
		 * Notice that a context of the receiver has been killed
		 *
		 * \param c  killed context
		 */
		void _context_killed(Signal_context * const c)
		{
			if (c->_deliver_fe.is_enqueued()) {
				_deliver.remove(&c->_deliver_fe);
			}
			_contexts.remove(&c->_contexts_fe);
		}


		/***************************
		 ** Signal_context_killer **
		 ***************************/

		void _signal_context_kill_pending() { _context_kills++; }

		void _signal_context_kill_done()
		{
			_context_kills--;
			if (!_context_kills && _killer) {
				this->~Signal_receiver();
				_killer->_signal_receiver_kill_done();
			}
		}

	public:

		/**
		 * Constructor
		 */

		/**
		 * Let a handler 'h' wait for signals of the receiver
		 *
		 * \retval  0 succeeded
		 * \retval -1 failed
		 */
		int add_handler(Signal_handler * const h)
		{
			if (_killer) { return -1; }
			_handlers.enqueue(&h->_handlers_fe);
			h->_await_signal(this);
			_listen();
			return 0;
		}

		/**
		 * Stop a handler 'h' from waiting for signals of the receiver
		 */
		void remove_handler(Signal_handler * const h)
		{
			_handlers.remove(&h->_handlers_fe);
		}

		/**
		 * Create a context that is assigned to the receiver
		 *
		 * \retval  0  succeeded
		 * \retval -1  failed
		 */
		int new_context(void * p, unsigned imprint)
		{
			if (_killer) { return -1; }
			new (p) Signal_context(this, imprint);
			Signal_context * const c = (Signal_context *)p;
			_contexts.enqueue(&c->_contexts_fe);
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
			if (_killer) { return -1; }

			/* start killing at all contexts of the receiver */
			Signal_context * c = _contexts.dequeue()->object();
			while (c) {
				c->kill(this);
				c = _contexts.dequeue()->object();
			}
			/* destruct directly if no context kill is pending */
			if (!_context_kills) {
				this->~Signal_receiver();
				return 0;
			}
			/* wait for pending context kills */
			_killer = k;
			_killer->_signal_receiver_kill_pending();
			return 0;
		}
};

#endif /* _KERNEL__SIGNAL_RECEIVER_ */
