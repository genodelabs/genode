/*
 * \brief  Platform driver - shared interrupts
 * \author Stefan Kalkowski
 * \date   2022-09-27
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__SHARED_IRQ_H_
#define _SRC__DRIVERS__PLATFORM__SHARED_IRQ_H_

#include <base/registry.h>
#include <base/rpc_server.h>
#include <irq_session/connection.h>

namespace Driver {
	using namespace Genode;

	class Shared_interrupt;
	class Shared_interrupt_session;
}


class Driver::Shared_interrupt :
	private Registry<Shared_interrupt>::Element
{
	private:

		Env                               & _env;
		unsigned                            _number;
		Io_signal_handler<Shared_interrupt> _handler;
		Constructible<Irq_connection>       _irq {};
		Registry<Shared_interrupt_session>  _sessions {};

		void _handle();

		friend class Shared_interrupt_session;

	public:

		Shared_interrupt(Registry<Shared_interrupt> & registry,
		                 Env                        & env,
		                 unsigned                     number)
		:
			Registry<Shared_interrupt>::Element(registry, *this),
			_env(env),
			_number(number),
			_handler(env.ep(), *this, &Shared_interrupt::_handle) {}

		unsigned number() const { return _number; }

		void enable(Irq_session::Trigger  mode,
		            Irq_session::Polarity polarity);
		void disable();
		void ack();
};


class Driver::Shared_interrupt_session :
	public  Rpc_object<Irq_session, Shared_interrupt_session>,
	private Registry<Shared_interrupt_session>::Element
{
	private:

		Rpc_entrypoint          & _ep;
		Shared_interrupt        & _sirq;
		Signal_context_capability _cap {};
		bool                      _outstanding { true };

	public:

		Shared_interrupt_session(Rpc_entrypoint       &ep,
		                         Shared_interrupt     &sirq,
		                         Irq_session::Trigger  mode,
		                         Irq_session::Polarity polarity)
		:
			Registry<Shared_interrupt_session>::Element(sirq._sessions, *this),
			_ep(ep),
			_sirq(sirq)
		{
			sirq.enable(mode, polarity);
			_ep.manage(this);
		}

		~Shared_interrupt_session()
		{
			_ep.dissolve(this);
			_sirq.disable();
		}

		bool outstanding() { return _outstanding; }
		void signal();


		/***************************
		 ** Irq session interface **
		 ***************************/

		void ack_irq() override
		{
			_outstanding = false;
			_sirq.ack();
		}

		void sigh(Signal_context_capability cap) override { _cap = cap; }

		Info info() override
		{
			return { .type = Info::Type::INVALID, .address = 0, .value = 0 };
		}
};

#endif /* _SRC__DRIVERS__PLATFORM__SHARED_IRQ_H_ */
