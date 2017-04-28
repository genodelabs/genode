/*
 * \brief  Input-interrupt handler
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _IRQ_HANDLER_H_
#define _IRQ_HANDLER_H_

/* Genode includes */
#include <base/env.h>
#include <base/signal.h>
#include <irq_session/connection.h>

/* local includes */
#include "input_driver.h"
#include "serial_interface.h"

class Irq_handler
{
	private:

		Genode::Irq_connection               _irq;
		Genode::Signal_handler<Irq_handler>  _dispatcher;
		Input_driver                        &_input_driver;

		void _handle()
		{
			_irq.ack_irq();

			/* check for pending PS/2 input */
			while (_input_driver.event_pending())
				_input_driver.handle_event();
		}

	public:

		Irq_handler(Genode::Env &env, int irq_number,
		            Serial_interface &, Input_driver &input_driver)
		:
			_irq(env, irq_number),
			_dispatcher(env.ep(), *this, &Irq_handler::_handle),
			_input_driver(input_driver)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}
};

#endif /* _IRQ_HANDLER_H_ */
