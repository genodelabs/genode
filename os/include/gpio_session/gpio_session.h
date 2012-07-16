/*
 * \brief  Gpio session interface
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

#ifndef _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_
#define _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_

#include <base/signal.h>
#include <dataspace/capability.h>
#include <session/session.h>

namespace Gpio {

	using namespace Genode;

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Gpio"; }

		virtual ~Session() { }

		/**
		 * Configure direction on a specified GPIO pin as output
		 * 
		 * \param   gpio    number of the pin
		 * \param   enable  logic level on the pin
		 */
		virtual void direction_output(int gpio, bool enable) = 0;
		
		/**
		 * Configure direction on a specified GPIO pin as input
		 * 
		 * \param   gpio    number of the pin
		 */
		virtual void direction_input(int gpio) = 0;
		
		/**
		 * Set the logic level on a specified GPIO pin
		 * 
		 * \param   gpio    number of the pin
		 * \param   enable  logic level on the pin
		 */
		virtual void dataout(int gpio, bool enable) = 0;
		
		/**
		 * Read the logic level on a specified GPIO pin
		 * 
		 * \param   gpio    number of the pin
		 * 
		 * \return  level on specified GPIO pin
		 */
		virtual int  datain(int gpio) = 0;
		
		/**
		 * Enable the debounce on a specified input GPIO pin
		 * 
		 * \param   gpio    number of the pin
		 */
		virtual void debounce_enable(int gpio, bool enable) = 0;
		
		/**
		 * Set the debouncing time for all input pins of GPIO bank
		 *
		 * \param   gpio    number of the pin
		 */
		virtual void debouncing_time(int gpio, unsigned int us) = 0;
		
		/**
		 * Configure the interrupt request on occurence of a falling edge on 
		 * a specified input GPIO pin
		 * 
		 * \param   gpio    number of the pin
		 */
		virtual void falling_detect(int gpio, bool enable) = 0;
		
		/**
		 * Configure the interrupt request on occurence of a rising edge on 
		 * a specified input GPIO pin
		 * 
		 * \param   gpio    number of the pin
		 */
		virtual void rising_detect(int gpio, bool enable) = 0;
		
		/**
		 * Enable or disable the interrupt on a specified GPIO pin
		 * 
		 * \param   gpio    number of the pin
		 * \param   enable  interrupt status( true - enable, false - disable)
		 */
		virtual void irq_enable(int gpio, bool enable) = 0;

		/**
		 * Register signal handler to be notified on interrupt on a specified 
		 * GPIO pin
		 * 
		 * \param   cap     capability of signal-context to handle GPIO 
		 *                  interrupt
		 * \param   gpio    number of the pin
		 * 
		 * This function is used for a set up signal handler for a specified 
		 * GPIO interrupt. Signal emited to the client if IRQ on pin configured   
		 * when the driver handles this IRQ.
		 */
		virtual void irq_sigh(Signal_context_capability cap, int gpio) = 0;

		/*******************
		 ** RPC interface **
		 *******************/

		GENODE_RPC(Rpc_direction_output, void, direction_output, int, bool);
		GENODE_RPC(Rpc_direction_input, void, direction_input, int);
		GENODE_RPC(Rpc_dataout, void, dataout, int, bool);
		GENODE_RPC(Rpc_datain, int,  datain, int);
		GENODE_RPC(Rpc_debounce_enable, void, debounce_enable, int, bool);
		GENODE_RPC(Rpc_debouncing_time, void, debouncing_time, int, unsigned int);
		GENODE_RPC(Rpc_falling_detect, void, falling_detect, int, bool);
		GENODE_RPC(Rpc_rising_detect, void, rising_detect, int, bool);
		GENODE_RPC(Rpc_irq_enable, void, irq_enable, int, bool);
		GENODE_RPC(Rpc_irq_sigh, void, irq_sigh, Signal_context_capability, int);


		typedef Meta::Type_tuple<Rpc_direction_output,
				Meta::Type_tuple<Rpc_direction_input,
				Meta::Type_tuple<Rpc_dataout,
				Meta::Type_tuple<Rpc_datain,
				Meta::Type_tuple<Rpc_debounce_enable,
				Meta::Type_tuple<Rpc_debouncing_time,
				Meta::Type_tuple<Rpc_falling_detect,
				Meta::Type_tuple<Rpc_rising_detect,
				Meta::Type_tuple<Rpc_irq_enable,
				Meta::Type_tuple<Rpc_irq_sigh,
								Meta::Empty>
				> > > > > > > > > Rpc_functions;
	};
}

#endif /* _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_ */
