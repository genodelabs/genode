/*
 * \brief  Gpio driver for the RaspberryPI
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \date   2015-07-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <base/printf.h>
#include <drivers/board_base.h>
#include <gpio/driver.h>
#include <irq_session/connection.h>

/* local includes */
#include <irq.h>
#include "gpio.h"

static int verbose = 1;

class Rpi_driver : public Gpio::Driver
{
	private:

		enum {
			MAX_PINS  = 54
		};

		Server::Entrypoint                     &_ep;
		Gpio_reg                               _reg;
		Genode::Irq_connection                 _irq;
		Genode::Signal_rpc_member<Rpi_driver>  _dispatcher;
		Genode::Signal_context_capability      _sig_cap[MAX_PINS];
		bool                                   _irq_enabled[MAX_PINS];
		bool                                   _async;

		void _handle(unsigned)
		{
			handle_irq();
		}

		Rpi_driver(Server::Entrypoint &ep)
		:	_ep(ep),
			_reg(Genode::Board_base::GPIO_CONTROLLER_BASE,0x0,Genode::Board_base::GPIO_CONTROLLER_SIZE),
			_irq(Gpio::IRQ),
			_dispatcher(ep,*this,&Rpi_driver::_handle),
			_async(false)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}

	public:

		void handle_irq()
		{
			unsigned long status = _reg.get_gpio_status0();
			for(unsigned i = 0; i < 32; i++) {
				if ((status & (1 << i)) && _irq_enabled[i] &&
					_sig_cap[i].valid())
					Genode::Signal_transmitter(_sig_cap[i]).submit();
			}
			status = _reg.get_gpio_status1();
			for(unsigned i = 0; i < 22; i++) {
				if ((status & (1 << i)) && _irq_enabled[32+i] &&
					_sig_cap[32+i].valid())
					Genode::Signal_transmitter(_sig_cap[32+i]).submit();
			}
		}

		void set_async_events(bool async)
		{
			_async = async;
		}

		void set_custom_function(unsigned gpio, unsigned function)
		{
			if (verbose) PDBG("gpio=%d function=%d", gpio, function);

			_reg.set_gpio_function(gpio, function);
		}

		static Rpi_driver& factory(Server::Entrypoint &ep);


		/******************************
		 **  Gpio::Driver interface  **
		 ******************************/

		void direction(unsigned gpio, bool input)
		{
			if (verbose) PDBG("gpio=%d input=%d", gpio, input);

			_reg.set_gpio_function(gpio, (input)?Gpio_reg::GPIO_FSEL_INPUT:Gpio_reg::GPIO_FSEL_OUTPUT);
		}

		void write(unsigned gpio, bool level)
		{
			if (verbose) PDBG("gpio=%d level=%d", gpio, level);

			if(_reg.get_gpio_function(gpio)!=Gpio_reg::GPIO_FSEL_OUTPUT)
				PWRN("GPIO pin (%d) is not configured for output.",gpio);

			if(level)
				_reg.set_gpio_level(gpio);
			else
				_reg.clear_gpio_level(gpio);
		}

		bool read(unsigned gpio)
		{
			if(_reg.get_gpio_function(gpio)!=Gpio_reg::GPIO_FSEL_INPUT)
				PWRN("GPIO pin (%d) is not configured for input.",gpio);

			return _reg.get_gpio_level(gpio);
		}

		void debounce_enable(unsigned gpio, bool enable)
		{
			PWRN("Not supported!");
		}

		void debounce_time(unsigned gpio, unsigned long us)
		{
			PWRN("Not supported!");
		}

		void falling_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			if(_async)
				_reg.set_gpio_async_falling_detect(gpio);
			else
				_reg.set_gpio_falling_detect(gpio);
		}

		void rising_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			if(_async)
				_reg.set_gpio_async_rising_detect(gpio);
			else
				_reg.set_gpio_rising_detect(gpio);
		}

		void high_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			_reg.set_gpio_high_detect(gpio);
		}

		void low_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			_reg.set_gpio_low_detect(gpio);
		}

		void irq_enable(unsigned gpio, bool enable)
		{
			if (verbose) PDBG("gpio=%d enable=%d", gpio, enable);

			_irq_enabled[gpio] = enable;
		}

		void ack_irq(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			_reg.clear_event(gpio);
			_irq.ack_irq();
		}

		void register_signal(unsigned gpio,
		                     Genode::Signal_context_capability cap)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			_sig_cap[gpio] = cap;
		}

		void unregister_signal(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);
			Genode::Signal_context_capability cap;

			_sig_cap[gpio] = cap;
		}

		bool gpio_valid(unsigned gpio) { return gpio < (MAX_PINS); }
};


Rpi_driver& Rpi_driver::factory(Server::Entrypoint &ep)
{
	static Rpi_driver driver(ep);
	return driver;
}

#endif /* _DRIVER_H_ */
