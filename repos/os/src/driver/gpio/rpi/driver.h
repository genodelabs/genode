/*
 * \brief  Gpio driver for the RaspberryPI
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \date   2015-07-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__GPIO__SPEC__RPI__DRIVER_H_
#define _DRIVERS__GPIO__SPEC__RPI__DRIVER_H_

/* Genode includes */
#include <base/log.h>
#include <gpio/driver.h>
#include <irq_session/connection.h>

/* local includes */
#include <gpio.h>

static int verbose = 1;

namespace Gpio { class Rpi_driver; }

namespace Rpi {
	enum { GPIO_CONTROLLER_BASE = 0x20200000,
	       GPIO_CONTROLLER_SIZE = 0x1000      };
};

class Gpio::Rpi_driver : public Driver
{
	using Pin = Gpio::Pin;

	private:

		enum {
			IRQ      = 49,
			MAX_PINS = 54,
		};

		Reg                                    _reg;
		Genode::Irq_connection                 _irq;
		Genode::Signal_handler<Rpi_driver>     _dispatcher;
		Genode::Signal_context_capability      _sig_cap[MAX_PINS];
		bool                                   _irq_enabled[MAX_PINS];
		bool                                   _async;

		void _handle()
		{
			_reg.for_each_gpio_status([&] (unsigned i, bool s) {
				if (!s || !_irq_enabled[i] || !_sig_cap[i].valid()) { return; }
				Genode::Signal_transmitter(_sig_cap[i]).submit();
			});
		}

		void _invalid_gpio(Pin gpio) {
			Genode::error("invalid GPIO pin number ", gpio); }

	public:

		Rpi_driver(Genode::Env &env)
		:
			_reg(env, Rpi::GPIO_CONTROLLER_BASE, 0, Rpi::GPIO_CONTROLLER_SIZE),
			_irq(env, unsigned(IRQ)),
			_dispatcher(env.ep(), *this, &Rpi_driver::_handle),
			_async(false)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}

		void set_async_events(bool async) { _async = async; }

		void set_func(unsigned gpio, Reg::Function function)
		{
			if (verbose)
				Genode::log("set_func: gpio=", gpio, " function=", (int)function);

			_reg.set_gpio_function(gpio, function);
		}


		/******************************
		 **  Driver interface  **
		 ******************************/

		bool gpio_valid(Pin gpio) override { return gpio.value < MAX_PINS; }

		void direction(Pin gpio, bool input) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose)
				Genode::log("direction: gpio=", gpio, " input=", input);

			Reg::Function f = input ? Reg::FSEL_INPUT : Reg::FSEL_OUTPUT;
			_reg.set_gpio_function(gpio.value, f);
		}

		void write(Pin gpio, bool level) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose)
				Genode::log("write: gpio=", gpio, " level=", level);

			if (_reg.get_gpio_function(gpio.value)!=Reg::FSEL_OUTPUT)
				warning("GPIO pin ", gpio, " is not configured for output");

			if (level)
				_reg.set_gpio_level(gpio.value);
			else
				_reg.clear_gpio_level(gpio.value);
		}

		bool read(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return 0; }
			if(_reg.get_gpio_function(gpio.value) != Reg::FSEL_INPUT)
				warning("GPIO pin ", gpio, " is not configured for input");

			return _reg.get_gpio_level(gpio.value);
		}

		void debounce_enable(Pin, bool) override {
			Genode::warning("debounce_enable not supported!"); }

		void debounce_time(Pin, unsigned long) override {
			Genode::warning("debounce_time not supported!"); }

		void falling_detect(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose) Genode::log("falling_detect: gpio=", gpio);

			if(_async)
				_reg.set_gpio_async_falling_detect(gpio.value);
			else
				_reg.set_gpio_falling_detect(gpio.value);
		}

		void rising_detect(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose) Genode::log("rising_detect: gpio=", gpio);

			if(_async)
				_reg.set_gpio_async_rising_detect(gpio.value);
			else
				_reg.set_gpio_rising_detect(gpio.value);
		}

		void high_detect(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("high_detect: gpio=", gpio);

			_reg.set_gpio_high_detect(gpio.value);
		}

		void low_detect(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("low_detect: gpio=", gpio);

			_reg.set_gpio_low_detect(gpio.value);
		}

		void irq_enable(Pin gpio, bool enable) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("irq_enable: gpio=", gpio, " enable=", enable);

			_irq_enabled[gpio.value] = enable;
		}

		void ack_irq(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("ack_irq: gpio=", gpio);

			_reg.clear_event(gpio.value);
			_irq.ack_irq();
		}

		void register_signal(Pin gpio,
		                     Genode::Signal_context_capability cap) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("register_signal: gpio=", gpio);

			_sig_cap[gpio.value] = cap;
		}

		void unregister_signal(Pin gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("unregister_signal: gpio=", gpio);
			Genode::Signal_context_capability cap;

			_sig_cap[gpio.value] = cap;
		}
};

#endif /* _DRIVERS__GPIO__SPEC__RPI__DRIVER_H_ */
