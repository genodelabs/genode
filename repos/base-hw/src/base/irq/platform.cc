/*
 * \brief  Client-side IRQ session interface - specific for base-hw
 * \author Martin Stein
 * \date   2013-10-24
 */

/*
 * Copyright (C) 2013-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <irq_session/client.h>

void Genode::Irq_session_client::wait_for_irq()
{
	while (Kernel::await_signal(irq_signal.receiver_id,
	                            irq_signal.context_id))
	{
		PERR("failed to receive interrupt");
	}
}
