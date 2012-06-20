/*
 * \brief  Main-signal receiver and signal-helper functions
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <base/env.h>
#include <base/printf.h>
#include <base/signal.h>

#include "routine.h"

/**
 * Context base for IRQ, Timer, etc.
 */
class Driver_context : public Genode::Signal_context
{
	public:

		/**
		 * Perform context operation
		 */
		virtual void handle() = 0;

		virtual char const *debug() = 0;
};


/**
 * This singelton currently received all signals
 */
class Service_handler
{

	private:

		Genode::Signal_receiver *_receiver;

		Service_handler() { }

	public:

		static Service_handler * s()
		{
			static Service_handler _s;
			return &_s;
		}

		void receiver(Genode::Signal_receiver *recv) { _receiver = recv; }

		/**
		 * Dispatch for wait for signal
		 */
		void process()
		{
			if (Routine::all()) {
				Routine::schedule();
				return;
			}

			do {
				Genode::Signal s = _receiver->wait_for_signal();

				/* handle signal IRQ, timer, or event signals */
				Driver_context *ctx = static_cast<Driver_context *>(s.context());
				ctx->handle();
			} while (_receiver->pending());
		}
};


/**
 * Helper that holds sender and receiver
 */
class Signal_helper
{
	private:

		Genode::Signal_receiver    *_receiver;
		Genode::Signal_transmitter *_sender;
	
	public:

		Signal_helper(Genode::Signal_receiver *recv)
		: _receiver(recv),
		  _sender(new (Genode::env()->heap()) Genode::Signal_transmitter()) { }

		Genode::Signal_receiver    *receiver() const { return _receiver; }
		Genode::Signal_transmitter *sender() const { return _sender; }
};


namespace Timer
{
	void init(Genode::Signal_receiver *recv);
}

namespace Irq
{
	void init(Genode::Signal_receiver *recv);
}

namespace Event
{
	void init(Genode::Signal_receiver *recv);
}

namespace Storage
{
	void init(Genode::Signal_receiver *recv);
}

#endif /* _SIGNAL_H_ */
