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

#include <base/thread.h>
#include <irq_session/connection.h>

#include "input_driver.h"

class Irq_handler : Genode::Thread<4096>
{
	private:

		Genode::Irq_connection  _irq;
		Input_driver           &_input_driver;

	public:

		Irq_handler(int irq_number, Input_driver &input_driver)
		:
			Thread("irq_handler"),
			_irq(irq_number),
			_input_driver(input_driver)
		{
			start();
		}

		void entry()
		{
			while (1) {
				_irq.wait_for_irq();
				while (_input_driver.event_pending())
					_input_driver.handle_event();
			}
		}
};

#endif /* _IRQ_HANDLER_H_ */
