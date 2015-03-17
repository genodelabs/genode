/*
 * \brief  Connection to IRQ service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IRQ_SESSION__CONNECTION_H_
#define _INCLUDE__IRQ_SESSION__CONNECTION_H_

#include <irq_session/client.h>
#include <base/connection.h>

namespace Genode { struct Irq_connection; }

struct Genode::Irq_connection : Connection<Irq_session>, Irq_session_client
{

	private:

		Genode::Signal_receiver           _sig_rec;
		Genode::Signal_context            _sigh_ctx;
		Genode::Signal_context_capability _sigh_cap;

	public:

		/**
		 * Constructor
		 *
		 * \param irq      physical interrupt number
		 * \param trigger  interrupt trigger (e.g., level/edge)
		 * \param polarity interrupt trigger polarity (e.g., low/high)
		 */
		Irq_connection(unsigned irq,
		               Irq_session::Trigger  trigger  = Irq_session::TRIGGER_UNCHANGED,
		               Irq_session::Polarity polarity = Irq_session::POLARITY_UNCHANGED)
		:
			Connection<Irq_session>(
				session("ram_quota=4K, irq_number=%u, irq_trigger=%u, irq_polarity=%u",
				        irq, trigger, polarity)),
			Irq_session_client(cap()),
			_sigh_cap(_sig_rec.manage(&_sigh_ctx))
		{
			/* register default signal handler */
			Irq_session_client::sigh(_sigh_cap);
		}

		~Irq_connection()
		{
			Irq_session_client::sigh(Genode::Signal_context_capability());
			_sig_rec.dissolve(&_sigh_ctx);
		}

		/**
		 * Convenience function to acknowledge last IRQ and to block calling
		 * thread until next IRQ fires.
		 */
		void wait_for_irq();
};

#endif /* _INCLUDE__IRQ_SESSION__CONNECTION_H_ */
