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

		Genode::Env                           &_env;
		Genode::Irq_connection                 _irq;
		Genode::Io_signal_handler<Irq_handler> _handler;

		unsigned _sem_cnt = 1;

		void _handle() { _sem_cnt = 0; }

	public:

		Irq_handler(Genode::Env &env, int irq_number)
		:
			_env(env), _irq(env, irq_number),
			_handler(env.ep(), *this, &Irq_handler::_handle)
		{
			_irq.sigh(_handler);
			_irq.ack_irq();
		}

		void wait()
		{
			_sem_cnt++;
			while (_sem_cnt > 0)
				_env.ep().wait_and_dispatch_one_io_signal();
		}

		void ack() { _irq.ack_irq(); }
};

#endif /* _IRQ_HANDLER_H_ */
