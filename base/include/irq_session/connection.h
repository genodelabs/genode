/*
 * \brief  Connection to IRQ service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IRQ_SESSION__CONNECTION_H_
#define _INCLUDE__IRQ_SESSION__CONNECTION_H_

#include <irq_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Irq_connection : Connection<Irq_session>, Irq_session_client
	{
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
			Irq_session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__IRQ_SESSION__CONNECTION_H_ */
