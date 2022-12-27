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

#ifndef _DRIVERS__INPUT__SPEC__PS2__IRQ_HANDLER_H_
#define _DRIVERS__INPUT__SPEC__PS2__IRQ_HANDLER_H_

/* Genode includes */
#include <base/entrypoint.h>
#include <platform_session/device.h>

/* local includes */
#include "input_driver.h"

class Irq_handler
{
	private:

		Platform::Device::Irq               _irq;
		Genode::Signal_handler<Irq_handler> _handler;
		Input_driver                       &_input_driver;
		Event::Session_client              &_event_session;

		void _handle()
		{
			_irq.ack();

			_event_session.with_batch([&] (Event::Session_client::Batch &batch) {
				while (_input_driver.event_pending())
					_input_driver.handle_event(batch);
			});
		}

	public:

		Irq_handler(Genode::Entrypoint    &ep,
		            Input_driver          &input_driver,
		            Event::Session_client &event_session,
		            Platform::Device      &device,
		            unsigned               idx)
		:
			_irq(device, {idx}),
			_handler(ep, *this, &Irq_handler::_handle),
			_input_driver(input_driver),
			_event_session(event_session)
		{
			_irq.sigh(_handler);
		}
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__IRQ_HANDLER_H_ */
