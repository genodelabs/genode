/*
 * \brief  UART driver interface
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TERMINAL_DRIVER_H_
#define _TERMINAL_DRIVER_H_

namespace Terminal {

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
		 * Write character to terminal
		 */
		virtual void put_char(char c) = 0;

		/**
		 * Return true if character is available for reading
		 */
		virtual bool char_avail() = 0;

		/**
		 * Read character from terminal
		 */
		virtual char get_char() = 0;
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
		 * \param callback  functor called when data becomes available for
		 *                  reading
		 *
		 * \throws Uart_not_available
		 *
		 * Note that the 'callback' is called in the context of the IRQ
		 * handler. Hence, the operations performed by the registered
		 * function must be properly synchronized.
		 */
		virtual Driver *create(unsigned index, Char_avail_callback &callback) = 0;

		/**
		 * Destroy driver
		 */
		virtual void destroy(Driver *driver) = 0;
	};

}

#endif /* _TERMINAL_DRIVER_H_ */
