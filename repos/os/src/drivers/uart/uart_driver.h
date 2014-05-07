/*
 * \brief  UART driver interface
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

namespace Uart {

	/**
	 * Functor, called by 'Driver' when data is ready for reading
	 */
	struct Char_avail_callback
	{
		virtual void operator()() { }
	};

	struct Driver
	{
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
		virtual void baud_rate(int /*bits_per_second*/)
		{
			PINF("Setting baudrate is not supported yet. Use default 115200.");
		}
	};

	/**
	 * Interface for constructing the driver objects
	 */
	struct Driver_factory
	{
		struct Not_available { };

		/**
		 * Construct new driver
		 *
		 * \param index     index of UART to access
		 * \param baudrate  baudrate of UART
		 * \param callback  functor called when data becomes available for
		 *                  reading
		 *
		 * \throws Uart_not_available
		 *
		 * Note that the 'callback' is called in the context of the IRQ
		 * handler. Hence, the operations performed by the registered
		 * function must be properly synchronized.
		 */
		virtual Driver *create(unsigned index, unsigned baudrate, Char_avail_callback &callback) = 0;

		/**
		 * Destroy driver
		 */
		virtual void destroy(Driver *driver) = 0;
	};

}

#endif /* _UART_DRIVER_H_ */
