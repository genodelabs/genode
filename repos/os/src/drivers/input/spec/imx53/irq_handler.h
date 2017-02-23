/*
 * \brief  Input-interrupt handler
 * \author Josef Soentgen
 * \date   2015-04-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _IRQ_HANDLER_H_
#define _IRQ_HANDLER_H_

/* Genode includes */
#include <irq_session/connection.h>

class Irq_handler
{
	private:

		Genode::Irq_connection                  _irq;
		Genode::Signal_receiver                 _sig_rec;
		Genode::Signal_dispatcher<Irq_handler>  _dispatcher;

		void _handle(unsigned) { }


	public:

		Irq_handler(Genode::Env &env, int irq_number)
		:
			_irq(env, irq_number),
			_dispatcher(_sig_rec, *this, &Irq_handler::_handle)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}

		void wait() { _sig_rec.wait_for_signal(); }

		void ack() { _irq.ack_irq(); }
};

#endif /* _IRQ_HANDLER_H_ */
