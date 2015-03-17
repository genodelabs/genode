/*
 * \brief  Client-side IRQ session interface
 * \author Christian Helmuth
 * \author Martin Stein
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	/*
	 * FIXME: This is used only client-internal and could thus be protected.
	 */
	Irq_signal const irq_signal;

	/**
	 * Constructor
	 *
	 * \param session  pointer to the session backend
	 */
	explicit Irq_session_client(Irq_session_capability const & session)
	:
		Rpc_client<Irq_session>(session),
		irq_signal(signal())
	{ }


	/*****************
	 ** Irq_session **
	 *****************/

	Irq_signal signal() override { return call<Rpc_signal>(); }

	void wait_for_irq() override;
};

#endif /* _INCLUDE__IRQ_SESSION__CLIENT_H_ */
