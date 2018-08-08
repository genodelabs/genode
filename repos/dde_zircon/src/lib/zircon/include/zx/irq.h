/*
 * \brief  Zircon interrupt handler
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ZX_IRQ_H_
#define _ZX_IRQ_H_

#include <irq_session/connection.h>
#include <irq_session/client.h>
#include <base/signal.h>
#include <base/lock.h>

namespace ZX{
	enum {
		IRQ_LINES = 256
		/* we assume that a single driver will never need more than 256 interrupt lines */
	};

	template <typename tsession>
	class Irq
	{
		private:
			tsession _irq;
			Genode::Signal_handler<Irq> _irq_handler;
			Genode::Lock _lock;

			void _unlock()
			{
				_lock.unlock();
			}

		public:
			Irq(Genode::Env &env, int irq) :
				_irq(env, irq),
				_irq_handler(env.ep(), *this, &Irq::_unlock),
				_lock()
			{
				_irq.sigh(_irq_handler);
			}

			Irq(Genode::Env &env, Genode::Irq_session_capability cap) :
				_irq(cap),
				_irq_handler(env.ep(), *this, &Irq::_unlock),
				_lock()
			{
				_irq.sigh(_irq_handler);
			}

			void wait()
			{
				_irq.ack_irq();
				_lock.lock();
			}
	};
}

#endif /* ifndef_ZX_IRQ_H_ */
