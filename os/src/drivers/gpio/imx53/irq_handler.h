/*
 * \brief  Gpio irq-handler
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
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
#include "driver.h"

class Irq_handler : Genode::Thread<4096>
{
private:

		int                     _irq_number;
		Genode::Irq_connection  _irq;
		Gpio::Driver           &_driver;

public:

		Irq_handler(int irq_number, Gpio::Driver &driver)
		: _irq_number(irq_number), _irq(irq_number), _driver(driver)
		{
			start();
		}

		void entry()
		{
			while (1) {
				_driver.handle_event(_irq_number);
				_irq.wait_for_irq();
			}
		}
};

#endif /* _IRQ_HANDLER_H_ */
