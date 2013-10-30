/*
 * \brief  Client-side IRQ session interface
 * \author Martin Stein
 * \date   2013-10-24
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IRQ_SESSION__CLIENT_H_
#define _IRQ_SESSION__CLIENT_H_

/* Genode includes */
#include <irq_session/capability.h>
#include <base/rpc_client.h>

namespace Genode
{
	/**
	 * Client-side IRQ session interface
	 */
	struct Irq_session_client : Rpc_client<Irq_session>
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

		Irq_signal signal() { return call<Rpc_signal>(); }

		void wait_for_irq()
		{
			while (Kernel::await_signal(irq_signal.receiver_id,
			                            irq_signal.context_id))
			{
				PERR("failed to receive interrupt");
			}
		}
	};
}

#endif /* _IRQ_SESSION__CLIENT_H_ */
