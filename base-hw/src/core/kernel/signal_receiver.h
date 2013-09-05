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
#include <assert.h>

namespace Kernel
{
	typedef Genode::Signal Signal;

	class Signal_receiver;

	template <typename T> class Fifo : public Genode::Fifo<T> { };

	class Signal_listener : public Fifo<Signal_listener>::Element
	{
		public:

			virtual void receive_signal(void * const signal_base,
			                            size_t const signal_size) = 0;
	};

	/**
	 * Specific signal type, owned by a receiver, can be triggered asynchr.
	 */
	class Signal_context : public Object<Signal_context, MAX_SIGNAL_CONTEXTS>,
	                       public Fifo<Signal_context>::Element
	{
		friend class Signal_receiver;

		Signal_receiver * const _receiver;  /* the receiver that owns us */
		unsigned const          _imprint;   /* every outgoing signals gets
		                                     * signed with this */
		unsigned                _submits;   /* accumul. undelivered submits */
		bool                    _await_ack; /* delivery ack pending */

		/*
		 * if not zero, _killer_id holds the ID of the actor
		 * that awaits the destruction of the signal context
		 */
		unsigned _killer_id;

		/**
		 * Utility to deliver all remaining submits
		 */
		inline void _deliver();

		/**
		 * Called by receiver when all submits have been delivered
		 */
		void _delivered()
		{
			_submits = 0;
			_await_ack = 1;
		}

		public:

			/**
			 * Constructor
			 */
			Signal_context(Signal_receiver * const r, unsigned const imprint) :
				_receiver(r), _imprint(imprint),
				_submits(0), _await_ack(0), _killer_id(0) { }

			/**
			 * Submit the signal
			 *
			 * \param n  number of submits
			 */
			void submit(unsigned const n)
			{
				assert(_submits < (unsigned)~0 - n);
				if (_killer_id) { return; }
				_submits += n;
				if (_await_ack) { return; }
				_deliver();
			}

			/**
			 * Acknowledge delivery of signal
			 *
			 * \retval   0  no kill request finished
			 * \retval > 0  name of finished kill request
			 */
			unsigned ack()
			{
				assert(_await_ack);
				_await_ack = 0;
				if (!_killer_id) {
					_deliver();
					return 0;
				}
				this->~Signal_context();
				return _killer_id;
			}

			/**
			 * Destruct or prepare to do it at next call of 'ack'
			 *
			 * \param killer_id  name of the kill request
			 *
			 * \return  wether destruction is done
			 */
			bool kill(unsigned const killer_id)
			{
				assert(!_killer_id);
				_killer_id = killer_id;
				if (_await_ack) { return 0; }
				this->~Signal_context();
				return 1;
			}
	};

	/**
	 * Manage signal contexts & enable external actors to trigger & await them
	 */
	class Signal_receiver :
		public Object<Signal_receiver, MAX_SIGNAL_RECEIVERS>
	{
		Fifo<Signal_listener> _listeners;
		Fifo<Signal_context> _pending_contexts;

		/**
		 * Deliver as much submitted signals to listeners as possible
		 */
		void _listen()
		{
			while (1)
			{
				/* any pending context? */
				if (_pending_contexts.empty()) return;
				Signal_context * const c = _pending_contexts.dequeue();

				/* if there is no listener, enqueue context again and return */
				if (_listeners.empty()) {
					_pending_contexts.enqueue(c);
					return;
				}
				/* awake a listener and transmit signal info to it */
				Signal_listener * const l = _listeners.dequeue();
				Signal::Data data((Genode::Signal_context *)c->_imprint,
				                  c->_submits);
				l->receive_signal(&data, sizeof(data));
				c->_delivered();
			}
		}

		public:

			/**
			 * Let a listener listen to the contexts of the receiver
			 */
			void add_listener(Signal_listener * const l)
			{
				_listeners.enqueue(l);
				_listen();
			}

			/**
			 * Stop a listener from listen to the contexts of the receiver
			 */
			void remove_listener(Signal_listener * const l) { _listeners.remove(l); }

			/**
			 * Return wether any of the contexts of this receiver is pending
			 */
			bool pending() { return !_pending_contexts.empty(); }

			/**
			 * Recognize that context 'c' wants to be delivered
			 */
			void deliver(Signal_context * const c)
			{
				assert(c->_receiver == this);
				if (!c->is_enqueued()) _pending_contexts.enqueue(c);
				_listen();
			}
	};
}

void Kernel::Signal_context::_deliver()
{
	if (!_submits) return;
	_receiver->deliver(this);
}


#endif /* _KERNEL__SIGNAL_RECEIVER_ */
