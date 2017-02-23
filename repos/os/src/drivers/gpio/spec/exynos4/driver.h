/*
 * \brief  Gpio driver for the Odroid-x2
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-03
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <drivers/board_base.h>
#include <gpio/driver.h>
#include <irq_session/connection.h>
#include <timer_session/connection.h>

/* local includes */
#include <gpio.h>

namespace Gpio { class Odroid_x2_driver; }


class Gpio::Odroid_x2_driver : public Driver
{
	private:

		Reg                                         _reg1;
		Reg                                         _reg2;
		Reg                                         _reg3;
		Reg                                         _reg4;
		Genode::Irq_connection                      _irq;
		Genode::Signal_handler<Odroid_x2_driver>    _dispatcher;
		Genode::Signal_context_capability           _sig_cap[MAX_PINS];
		bool                                        _irq_enabled[MAX_PINS];
		bool                                        _async;

		void _handle()
		{
			handle_irq();
		}

		void handle_irq() { }

		Gpio::Reg *_gpio_reg(int gpio_pin)
		{
			int pos = gpio_bank_index(gpio_pin, true);
			switch(pos) {
				case 0 ... 13:
					return &_reg1;
				case 14 ... 38:
					return &_reg2;
				case 39:
					return &_reg3;
				case 40 ... 46:
					return &_reg4;
				default:
					Genode::error("no Gpio_bank for pin ", gpio_pin, " available");
					return 0;
			}
		}

		int _gpio_index(int gpio) { return gpio & 0x1f; }

		Odroid_x2_driver(Genode::Env &env)
		:
		  _reg1(env, 0x11400000, 1000),
		  _reg2(env, 0x11000000, 1000),
		  _reg3(env, 0x03860000, 1000),
		  _reg4(env, 0x106E0000, 1000),
		  _irq(env, 104),
		  _dispatcher(env.ep(), *this, &Odroid_x2_driver::_handle),
		  _async(false)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}


	public:

		static Odroid_x2_driver& factory(Genode::Env &env);


		/******************************
		 **  Gpio::Driver interface  **
		 ******************************/

		void direction(unsigned gpio_pin, bool input)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _bank_offset[pos_gpio];
			int           gpio     = gpio_pin - sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			reg->set_direction(gpio, input, offset);
		}

		void write(unsigned gpio_pin, bool level)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _bank_offset[pos_gpio];
			int           gpio     = gpio_pin - sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			reg->write_pin(gpio, level, offset);
		}

		bool read(unsigned gpio_pin)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _bank_offset[pos_gpio];
			int           gpio     = gpio_pin - sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			return reg->read_pin(gpio, offset) ;

		}

		void debounce_enable(unsigned gpio, bool enable) {
			Genode::warning("debounce_enable not supported!"); }

		void debounce_time(unsigned gpio, unsigned long us) {
			Genode::warning("debounce_time not supported!"); }

		void falling_detect(unsigned gpio_pin)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _irq_offset[pos_gpio];
			int           gpio     = gpio_pin - sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			reg->set_enable_triggers(gpio, offset, FALLING);
		}

		void rising_detect(unsigned gpio_pin)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _irq_offset[pos_gpio];
			int           gpio     = gpio_pin - sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			reg->set_enable_triggers(gpio, offset, RISING);

		}

		void high_detect(unsigned gpio_pin)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _irq_offset[pos_gpio];
			int           gpio     = gpio_pin-sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			reg->set_enable_triggers(gpio, offset, HIGH);

		}

		void low_detect(unsigned gpio_pin)
		{
			int           pos_gpio = gpio_bank_index(gpio_pin, true);
			int           sum_gpio = gpio_bank_index(gpio_pin, false);
			Genode::off_t offset   = _irq_offset[pos_gpio];
			int           gpio     = gpio_pin - sum_gpio;

			Reg* reg = _gpio_reg(gpio_pin);
			reg->set_enable_triggers(gpio, offset, LOW);

		}

		void irq_enable(unsigned gpio_pin, bool enable)
		{
			_irq_enabled[gpio_pin] = enable;
		}

		void ack_irq(unsigned gpio_pin)
		{
			_irq.ack_irq();
		}

		void register_signal(unsigned gpio_pin,
		                     Genode::Signal_context_capability cap)
		{
			_sig_cap[gpio_pin] = cap;

		}

		void unregister_signal(unsigned gpio_pin)
		{
			Genode::Signal_context_capability cap;
			_sig_cap[gpio_pin] = cap;

		}

		int gpio_bank_index(int pin, bool pos)
		{
			int i = 0 ,sum = 0;

			while (i<MAX_BANKS && ((sum + _bank_sizes[i]) <= pin)) {
				sum +=  1 + _bank_sizes[i++];
			}
			return pos ? i : sum;
		}

		bool gpio_valid(unsigned gpio) { return gpio < (MAX_PINS); }
};

#endif /* _DRIVER_H_ */
