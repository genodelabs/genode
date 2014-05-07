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

#ifndef _IRQ_HANDLER_H_
#define _IRQ_HANDLER_H_

/* Genode includes */
#include <base/thread.h>
#include <irq_session/connection.h>

/* local includes */
#include "input_driver.h"
#include "serial_interface.h"

class Irq_handler : Genode::Thread<4096>
{
	private:

		Genode::Irq_connection  _irq;
		Serial_interface       *_channel;
		Input_driver           &_input_driver;

	public:

		Irq_handler(int irq_number, Serial_interface *channel, Input_driver &input_driver)
		:
			Thread("irq_handler"),
			_irq(irq_number),
			_channel(channel),
			_input_driver(input_driver)
		{
			start();
		}

		void entry()
		{
			while (1) {

#ifdef __CODEZERO__
				/*
				 * (Re-)enable device interrupt because Codezero tends to
				 * disable it in its IRQ handler.
				 */
				_channel->enable_irq();
#endif

				/* check for pending PS/2 input */
				while (_input_driver.event_pending())
					_input_driver.handle_event();

				/* block for new PS/2 data */
				_irq.wait_for_irq();
			}
		}
};

#endif /* _IRQ_HANDLER_H_ */
