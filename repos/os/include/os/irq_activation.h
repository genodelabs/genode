/*
 * \brief  IRQ handling utility
 * \author Christian Helmuth
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__IRQ_ACTIVATION_H_
#define _INCLUDE__OS__IRQ_ACTIVATION_H_

#include <base/thread.h>
#include <base/snprintf.h>
#include <irq_session/connection.h>

namespace Genode {

	struct Irq_handler
	{
		/**
		 * Called by IRQ activation on interrupt
		 */
		virtual void handle_irq(int irq_number) = 0;
	};

	/**
	 * Thread activated by IRQ
	 */
	class Irq_activation : Thread_base
	{
		private:

			int             _number;
			Irq_connection  _connection;
			Irq_handler    &_handler;
			char            _thread_name[8];

			char const *_create_thread_name(unsigned irq_number)
			{
				snprintf(_thread_name, sizeof(_thread_name), "irq.%02x", irq_number);
				return _thread_name;
			}

		public:

			/**
			 * Constructor
			 *
			 * \param irq_number  interrupt number
			 * \param handler     interrupt handler
			 * \param stack_size  size of stack or interrupt thread
			 */
			Irq_activation(int irq_number, Irq_handler &handler, size_t stack_size)
			:
				Thread_base(_create_thread_name(irq_number), stack_size),
				_number(irq_number), _connection(irq_number), _handler(handler)
			{
				start();
			}

			/**
			 * Thread entry function
			 *
			 * The interrupt thread infinitely waits for interrupts and calls
			 * the handler on occurrence.
			 */
			void entry()
			{
				while (true) {
					_connection.wait_for_irq();
					_handler.handle_irq(_number);
				}
			}
	};

}

#endif /* _INCLUDE__OS__IRQ_ACTIVATION_H_ */
