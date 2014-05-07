/*
 * \brief  Client-side IRQ session interface
 * \author Christian Helmuth
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

#include <irq_session/capability.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Irq_session_client : Rpc_client<Irq_session>
	{
		explicit Irq_session_client(Irq_session_capability session)
		: Rpc_client<Irq_session>(session) { }

		void wait_for_irq() { call<Rpc_wait_for_irq>(); }
	};
}

#endif /* _INCLUDE__IRQ_SESSION__CLIENT_H_ */
