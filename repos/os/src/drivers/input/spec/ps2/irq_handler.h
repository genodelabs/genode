/*
 * \brief  Input-interrupt handler
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__IRQ_HANDLER_H_
#define _DRIVERS__INPUT__SPEC__PS2__IRQ_HANDLER_H_

/* Genode includes */
#include <base/thread.h>
#include <irq_session/connection.h>
#include <os/server.h>

/* local includes */
#include "input_driver.h"

class Irq_handler
{
	private:

		Genode::Irq_session_client              _irq;
		Genode::Signal_rpc_member<Irq_handler>  _dispatcher;
		Input_driver                           &_input_driver;

		void _handle(unsigned)
		{
			_irq.ack_irq();

			while (_input_driver.event_pending())
				_input_driver.handle_event();
		}

	public:

		Irq_handler(Server::Entrypoint &ep, Input_driver &input_driver,
		            Genode::Irq_session_capability irq_cap)
		:
			_irq(irq_cap),
			_dispatcher(ep, *this, &Irq_handler::_handle),
			_input_driver(input_driver)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__IRQ_HANDLER_H_ */
