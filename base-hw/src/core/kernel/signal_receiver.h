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
	 * Enables external components to act as a signal handler
	 */
	class Signal_handler;

	/**
	 * Signal types that are assigned to a signal receiver each
	 */
	class Signal_context;

	/**
	 * Combines signal contexts to an entity that handlers can listen to
	 */
	class Signal_receiver;

	/**
	 * Signal delivery backend
	 *
	 * \param dst   destination
	 * \param base  signal-data base
	 * \param size  signal-data size
	 */
	void deliver_signal(Signal_handler * const dst,
	                    void * const           base,
	                    size_t const           size);
}

class Kernel::Signal_handler
{
	friend class Signal_receiver;

	private:

		typedef Genode::Fifo_element<Signal_handler> Fifo_element;

		Fifo_element   _fe;
		unsigned const _id;

	public:

		/**
		 * Constructor
		 */
		Signal_handler(unsigned id) : _fe(this), _id(id) { }


		/***************
		 ** Accessors **
		 ***************/

		unsigned id() { return _id; }
};

class Kernel::Signal_context
:
	public Object<Signal_context, MAX_SIGNAL_CONTEXTS>
{
	friend class Signal_receiver;

	private:

		typedef Genode::Fifo_element<Signal_context> Fifo_element;

		Fifo_element            _fe;
		Signal_receiver * const _receiver;
		unsigned const          _imprint;
		unsigned                _submits;
		bool                    _ack;
		unsigned                _killer;

		/**
		 * Tell receiver about the submits of the context if any
		 */
		inline void _deliverable();

		/**
		 * Called by receiver when all submits have been delivered
		 */
		void _delivered()
		{
			_submits = 0;
			_ack     = 0;
		}

	public:

		/**
		 * Constructor
		 */
		Signal_context(Signal_receiver * const r, unsigned const imprint)
		:
			_fe(this), _receiver(r), _imprint(imprint), _submits(0), _ack(1),
			_killer(0)
		{ }

		/**
		 * Submit the signal
		 *
		 * \param n  number of submits
		 */
		void submit(unsigned const n)
		{
			if (_submits >= (unsigned)~0 - n) {
				PERR("overflow at signal-submit count");
				return;
			}
			if (_killer) {
				PERR("signal context already in destruction");
				return;
			}
			_submits += n;
			if (!_ack) { return; }
			_deliverable();
		}

		/**
		 * Acknowledge delivery of signal
		 *
		 * \retval   0  no kill request finished
		 * \retval > 0  name of finished kill request
		 */
		unsigned ack()
		{
			if (_ack) {
				PERR("unexpected signal acknowledgment");
				return 0;
			}
			if (!_killer) {
				_ack = 1;
				_deliverable();
				return 0;
			}
			this->~Signal_context();
			return _killer;
		}

		/**
		 * Destruct context or prepare to do it as soon as delivery is done
		 *
		 * \param killer  name of the kill request
		 *
		 * \retval 1  destruction is done
		 * \retval 0  destruction is initiated, will be done with the next ack
		 */
		bool kill(unsigned const killer)
		{
			/* FIXME: aggregate or avoid multiple kill requests */
			if (_killer) {
				PERR("multiple kill requests");
				while (1) { }
			}
			_killer = killer;
			if (!_ack) { return 0; }
			this->~Signal_context();
			return 1;
		}
};

class Kernel::Signal_receiver
:
	public Object<Signal_receiver, MAX_SIGNAL_RECEIVERS>
{
	friend class Signal_context;

	private:

		typedef Genode::Signal Signal;

		template <typename T> class Fifo : public Genode::Fifo<T> { };

		Fifo<Signal_handler::Fifo_element> _handlers;
		Fifo<Signal_context::Fifo_element> _deliverable;

		/**
		 * Recognize that context 'c' has submits to deliver
		 */
		void _add_deliverable(Signal_context * const c)
		{
			if (!c->_fe.is_enqueued()) _deliverable.enqueue(&c->_fe);
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
				if (_deliverable.empty()) return;
				Signal_context * const c = _deliverable.dequeue()->object();

				/* if there is no handler re-enqueue context and exit */
				if (_handlers.empty()) {
					_deliverable.enqueue(&c->_fe);
					return;
				}
				/* delivery from context to handler */
				Signal_handler * const h = _handlers.dequeue()->object();
				Signal::Data data((Genode::Signal_context *)c->_imprint,
				                   c->_submits);
				deliver_signal(h, &data, sizeof(data));
				c->_delivered();
			}
		}

	public:

		/**
		 * Let a handler wait for signals of the receiver
		 */
		void add_handler(Signal_handler * const h)
		{
			_handlers.enqueue(&h->_fe);
			_listen();
		}

		/**
		 * Stop a handler from waiting for signals of the receiver
		 */
		void remove_handler(Signal_handler * const h)
		{
			_handlers.remove(&h->_fe);
		}

		/**
		 * Return wether any of the contexts of this receiver is deliverable
		 */
		bool deliverable() { return !_deliverable.empty(); }
};


void Kernel::Signal_context::_deliverable()
{
	if (!_submits) return;
	_receiver->_add_deliverable(this);
}

#endif /* _KERNEL__SIGNAL_RECEIVER_ */
