/*
 * \brief  Client-side IRQ session interface
 * \author Christian Helmuth
 * \author Martin Stein
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IRQ_SESSION__CLIENT_H_
#define _INCLUDE__IRQ_SESSION__CLIENT_H_

/* Genode includes */
#include <irq_session/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Irq_session_client; }

/**
 * Client-side IRQ session interface
 */
struct Genode::Irq_session_client : Rpc_client<Irq_session>
{
	/**
	 * Constructor
	 *
	 * \param session  pointer to the session backend
	 */
	explicit Irq_session_client(Irq_session_capability const & session)
	:
		Rpc_client<Irq_session>(session)
	{ }


	/*****************
	 ** Irq_session **
	 *****************/

	void ack_irq() override { call<Rpc_ack_irq>(); }

	void sigh(Signal_context_capability sigh) override { call<Rpc_sigh>(sigh); }

	Info info() override { return call<Rpc_info>(); }
};

#endif /* _INCLUDE__IRQ_SESSION__CLIENT_H_ */
