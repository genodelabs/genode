/*
 * \brief  UART driver interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UART_DRIVER_BASE_H_
#define _UART_DRIVER_BASE_H_

#include <irq_session/connection.h>

namespace Uart {
	class  Driver_base;
	class  Driver;
	struct Driver_factory;
	struct Char_avail_functor;
};


/**
 * Functor, called by 'Driver' when data is ready for reading
 */
struct Uart::Char_avail_functor
{
	Genode::Signal_context_capability sigh;

	void operator ()() {
		if (sigh.valid()) Genode::Signal_transmitter(sigh).submit(); }

};


class Uart::Driver_base
{
	private:

		Char_avail_functor                 &_char_avail;
		Genode::Irq_connection              _irq;
		Genode::Signal_handler<Driver_base> _irq_handler;

	public:

		Driver_base(Genode::Env &env, int irq_number, Char_avail_functor &func)
		: _char_avail(func),
		  _irq(env, irq_number),
		  _irq_handler(env.ep(), *this, &Driver_base::handle_irq)
		{
			_irq.sigh(_irq_handler);
			_irq.ack_irq();
		}

		virtual ~Driver_base() {}

		/**
		 * Handle interrupt
		 */
		virtual void handle_irq()
		{
			_char_avail();
			_irq.ack_irq();
		}

		/**
		 * Write character to UART
		 */
		virtual void put_char(char c) = 0;

		/**
		 * Return true if character is available for reading
		 */
		virtual bool char_avail() = 0;

		/**
		 * Read character from UART
		 */
		virtual char get_char() = 0;

		/**
		 * Set baud rate for terminal
		 */
		virtual void baud_rate(size_t baud)
		{
			Genode::warning("Setting baudrate to ", baud,
			                " is not supported. Use default value.");
		}
};


/**
 * Interface for constructing the driver objects
 */
struct Uart::Driver_factory
{
	struct Not_available { };

	Genode::Env  &env;
	Genode::Heap &heap;
	Driver       *drivers[UARTS_NUM];

	Driver_factory(Genode::Env &env, Genode::Heap &heap)
	: env(env), heap(heap) {
		for (unsigned i = 0; i < UARTS_NUM; i++) drivers[i] = 0; }

	/**
	 * Construct new driver
	 *
	 * \param index     index of UART to access
	 * \param baudrate  baudrate of UART
	 * \param functor   functor called when data becomes available for
	 *                  reading
	 *
	 * \throws Uart_not_available
	 *
	 * Note that the 'callback' is called in the context of the IRQ
	 * handler. Hence, the operations performed by the registered
	 * function must be properly synchronized.
	 */
	Driver &create(unsigned index, unsigned baudrate,
	               Char_avail_functor &functor);

	/**
	 * Destroy driver
	 */
	void destroy(Driver &) { /* TODO */ }
};

#endif /* _UART_DRIVER_BASE_H_ */
