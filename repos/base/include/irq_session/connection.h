/*
 * \brief  Connection to IRQ service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IRQ_SESSION__CONNECTION_H_
#define _INCLUDE__IRQ_SESSION__CONNECTION_H_

#include <irq_session/client.h>
#include <base/connection.h>

namespace Genode { struct Irq_connection; }

struct Genode::Irq_connection : Connection<Irq_session>, Irq_session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Irq_session> _session(Parent               &,
	                                 unsigned              irq,
	                                 Irq_session::Trigger  trigger,
	                                 Irq_session::Polarity polarity,
	                                 Genode::addr_t        device_config_phys)
	{
		return session("ram_quota=6K, cap_quota=4, irq_number=%u, irq_trigger=%u, "
		               " irq_polarity=%u, device_config_phys=0x%lx",
		               irq, trigger, polarity, device_config_phys);
	}

	/**
	 * Constructor
	 *
	 * \param irq      physical interrupt number
	 * \param trigger  interrupt trigger (e.g., level/edge)
	 * \param polarity interrupt trigger polarity (e.g., low/high)
	 */
	Irq_connection(Env                  &env,
	               unsigned              irq,
	               Irq_session::Trigger  trigger  = Irq_session::TRIGGER_UNCHANGED,
	               Irq_session::Polarity polarity = Irq_session::POLARITY_UNCHANGED,
	               Genode::addr_t        device_config_phys = 0)
	:
		Connection<Irq_session>(env, _session(env.parent(), irq, trigger,
		                                      polarity, device_config_phys)),
		Irq_session_client(cap())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Irq_connection(unsigned irq,
	               Irq_session::Trigger  trigger  = Irq_session::TRIGGER_UNCHANGED,
	               Irq_session::Polarity polarity = Irq_session::POLARITY_UNCHANGED,
	               Genode::addr_t device_config_phys = 0) __attribute__((deprecated))
	:
		Connection<Irq_session>(_session(*Genode::env_deprecated()->parent(), irq,
		                                 trigger, polarity, device_config_phys)),
		Irq_session_client(cap())
	{ }
};

#endif /* _INCLUDE__IRQ_SESSION__CONNECTION_H_ */
