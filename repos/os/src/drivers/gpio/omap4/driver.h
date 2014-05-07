/*
 * \brief  Gpio driver for the OMAP4
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

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <drivers/board_base.h>
#include <gpio/driver.h>
#include <irq_session/connection.h>
#include <timer_session/connection.h>

/* local includes */
#include "gpio.h"

static int verbose = 0;


class Omap4_driver : public Gpio::Driver
{
	private:

		enum {
			MAX_BANKS = 6,
			MAX_PINS  = 32
		};


		struct Timer_delayer : Timer::Connection, Genode::Mmio::Delayer
		{
			/**
			 * Implementation of 'Delayer' interface
			 */
			void usleep(unsigned us) { Timer::Connection::usleep(us); }
		} _delayer;


		class Gpio_bank : public Genode::Thread<4096>
		{
			private:

				Gpio_reg                          _reg;
				Genode::Irq_connection            _irq;
				Genode::Signal_context_capability _sig_cap[MAX_PINS];
				bool                              _irq_enabled[MAX_PINS];

			public:

				Gpio_bank(Genode::addr_t base, Genode::size_t size,
				          unsigned irq)
				: Genode::Thread<4096>("irq handler"),
				  _reg(base, size), _irq(irq)
				{
					for (unsigned i = 0; i < MAX_PINS; i++)
						_irq_enabled[i] = false;
					start();
				}

				void entry()
				{
					unsigned long status;

					while (true) {
						_reg.write<Gpio_reg::Irqstatus_0>(0xffffffff);

						_irq.wait_for_irq();

						status = _reg.read<Gpio_reg::Irqstatus_0>();

						for(unsigned i = 0; i < MAX_PINS; i++) {
							if ((status & (1 << i)) && _irq_enabled[i] &&
							    _sig_cap[i].valid())
								Genode::Signal_transmitter(_sig_cap[i]).submit();
						}
					}
				}

				Gpio_reg* regs() { return &_reg; }

				void irq(int pin, bool enable)
				{
					if (enable) {
						_reg.write<Gpio_reg::Irqstatus_0>(1 << pin);
						_reg.write<Gpio_reg::Irqstatus_set_0>(1 << pin);
					}
					else
						_reg.write<Gpio_reg::Irqstatus_clr_0>(1 << pin);
					_irq_enabled[pin] = enable;
				}

				void sigh(int pin, Genode::Signal_context_capability cap) {
					_sig_cap[pin] = cap; }
		};


		static Gpio_bank _gpio_bank[MAX_BANKS];

		int _gpio_bank_index(int gpio)  { return gpio >> 5;   }
		int _gpio_index(int gpio)       { return gpio & 0x1f; }

		Omap4_driver()
		{
			for (int i = 0; i < MAX_BANKS; ++i) {
				if (verbose)
					PDBG("GPIO%d ctrl=%08x",
						 i+1, _gpio_bank[i].regs()->read<Gpio_reg::Ctrl>());
			}
		}

	public:

		static Omap4_driver& factory();


		/******************************
		 **  Gpio::Driver interface  **
		 ******************************/

		void direction(unsigned gpio, bool input)
		{
			if (verbose) PDBG("gpio=%d input=%d", gpio, input);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Oe>(input ? 1 : 0, _gpio_index(gpio));
		}

		void write(unsigned gpio, bool level)
		{
			if (verbose) PDBG("gpio=%d level=%d", gpio, level);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();

			if (level)
				gpio_reg->write<Gpio_reg::Setdataout>(1 << _gpio_index(gpio));
			else
				gpio_reg->write<Gpio_reg::Cleardataout>(1 << _gpio_index(gpio));
		}

		bool read(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			return gpio_reg->read<Gpio_reg::Datain>(_gpio_index(gpio));
		}

		void debounce_enable(unsigned gpio, bool enable)
		{
			if (verbose) PDBG("gpio=%d enable=%d", gpio, enable);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Debounceenable>(enable ? 1 : 0,
			                                          _gpio_index(gpio));
		}

		void debounce_time(unsigned gpio, unsigned long us)
		{
			if (verbose) PDBG("gpio=%d us=%ld", gpio, us);

			unsigned char debounce;

			if (us < 32)
				debounce = 0x01;
			else if (us > 7936)
				debounce = 0xff;
			else
				debounce = (us / 0x1f) - 1;

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Debouncingtime::Time>(debounce);
		}

		void falling_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Leveldetect0> (0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Leveldetect1> (0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Fallingdetect>(1, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Risingdetect> (0, _gpio_index(gpio));
		}

		void rising_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Leveldetect0> (0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Leveldetect1> (0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Fallingdetect>(0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Risingdetect> (1, _gpio_index(gpio));
		}

		void high_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Leveldetect0> (0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Leveldetect1> (1, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Fallingdetect>(0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Risingdetect> (0, _gpio_index(gpio));
		}

		void low_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Leveldetect0> (1, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Leveldetect1> (0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Fallingdetect>(0, _gpio_index(gpio));
			gpio_reg->write<Gpio_reg::Risingdetect> (0, _gpio_index(gpio));
		}

		void irq_enable(unsigned gpio, bool enable)
		{
			if (verbose) PDBG("gpio=%d enable=%d", gpio, enable);

			_gpio_bank[_gpio_bank_index(gpio)].irq(_gpio_index(gpio), enable);
		}

		void register_signal(unsigned gpio,
		                     Genode::Signal_context_capability cap)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			_gpio_bank[_gpio_bank_index(gpio)].sigh(_gpio_index(gpio), cap); }

		void unregister_signal(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Genode::Signal_context_capability cap;
			_gpio_bank[_gpio_bank_index(gpio)].sigh(_gpio_index(gpio), cap);
		}

		bool gpio_valid(unsigned gpio) { return gpio < (MAX_PINS*MAX_BANKS); }
};


Omap4_driver::Gpio_bank Omap4_driver::_gpio_bank[Omap4_driver::MAX_BANKS] = {
	{ Genode::Board_base::GPIO1_MMIO_BASE, Genode::Board_base::GPIO1_MMIO_SIZE,
	  Genode::Board_base::GPIO1_IRQ },
	{ Genode::Board_base::GPIO2_MMIO_BASE, Genode::Board_base::GPIO2_MMIO_SIZE,
	  Genode::Board_base::GPIO2_IRQ },
	{ Genode::Board_base::GPIO3_MMIO_BASE, Genode::Board_base::GPIO3_MMIO_SIZE,
	  Genode::Board_base::GPIO3_IRQ },
	{ Genode::Board_base::GPIO4_MMIO_BASE, Genode::Board_base::GPIO4_MMIO_SIZE,
	  Genode::Board_base::GPIO4_IRQ },
	{ Genode::Board_base::GPIO5_MMIO_BASE, Genode::Board_base::GPIO5_MMIO_SIZE,
	  Genode::Board_base::GPIO5_IRQ },
	{ Genode::Board_base::GPIO6_MMIO_BASE, Genode::Board_base::GPIO6_MMIO_SIZE,
	  Genode::Board_base::GPIO6_IRQ },
};


Omap4_driver& Omap4_driver::factory()
{
	static Omap4_driver driver;
	return driver;
}

#endif /* _DRIVER_H_ */
