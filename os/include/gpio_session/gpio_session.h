/*
 * \brief  Gpio session interface
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_
#define _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_

#include <base/signal.h>
#include <dataspace/capability.h>
#include <session/session.h>

namespace Gpio {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Gpio"; }

		enum Direction { IN, OUT };

		enum Irq_type  { LOW_LEVEL, HIGH_LEVEL, FALLING_EDGE, RISING_EDGE };


		virtual ~Session() { }

		/**
		 * Configure direction of the GPIO pin
		 *
		 * \param d  direction of the pin
		 */
		virtual void direction(Direction d) = 0;

		/**
		 * Write the logic level of the GPIO pin
		 *
		 * \param enable  logic level on the pin
		 */
		virtual void write(bool enable) = 0;

		/**
		 * Read the logic level on a specified GPIO pin
		 *
		 * \return  level on specified GPIO pin
		 */
		virtual bool read() = 0;

		/**
		 * Set the debouncing time
		 *
		 * \param us  debouncing time in microseconds, zero means no debouncing
		 */
		virtual void debouncing(unsigned int us) = 0;

		/**
		 * Configure the type of interrupt for the GPIO pin
		 *
		 * \param it  type of IRQ
		 */
		virtual void irq_type(Irq_type it) = 0;

		/**
		 * Enable or disable the interrupt of the GPIO pin
		 *
		 * \param enable  interrupt status( true - enable, false - disable)
		 */
		virtual void irq_enable(bool enable) = 0;

		/**
		 * Register signal handler to be notified on interrupt
		 *
		 * \param cap  capability of signal-context to handle GPIO interrupt
		 */
		virtual void irq_sigh(Genode::Signal_context_capability cap) = 0;


		/*******************
		 ** RPC interface **
		 *******************/

		GENODE_RPC(Rpc_direction,  void, direction, Direction);
		GENODE_RPC(Rpc_write,      void, write, bool);
		GENODE_RPC(Rpc_read,       bool, read);
		GENODE_RPC(Rpc_debouncing, void, debouncing, unsigned int);
		GENODE_RPC(Rpc_irq_type,   void, irq_type, Irq_type);
		GENODE_RPC(Rpc_irq_enable, void, irq_enable, bool);
		GENODE_RPC(Rpc_irq_sigh,   void, irq_sigh, Genode::Signal_context_capability);

		GENODE_RPC_INTERFACE(Rpc_direction, Rpc_write, Rpc_read,
		                     Rpc_debouncing, Rpc_irq_type, Rpc_irq_enable,
		                     Rpc_irq_sigh);
	};
}

#endif /* _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_ */
