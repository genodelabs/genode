/*
 * \brief  Gpio driver for the i.MX53
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-12-04
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


class Imx53_driver : public Gpio::Driver
{
	private:

		enum {
			MAX_BANKS = 7,
			MAX_PINS  = 32
		};


		struct Timer_delayer : Timer::Connection, Genode::Mmio::Delayer
		{
			/**
			 * Implementation of 'Delayer' interface
			 */
			void usleep(unsigned us) { Timer::Connection::usleep(us); }
		} _delayer;


		class Gpio_bank
		{
			public:

				void handle_irq();

			private:

				class Irq_handler : public Genode::Thread<4096>
				{
					private:

						Genode::Irq_connection _irq;
						Gpio_bank             *_bank;

					public:

						Irq_handler(unsigned irq, Gpio_bank *bank)
						:  Genode::Thread<4096>("irq handler"),
						   _irq(irq), _bank(bank) { start(); }

						void entry()
						{
							while (true) {
								_irq.wait_for_irq();
								_bank->handle_irq();
							}
						}

				};

				Gpio_reg                          _reg;
				Irq_handler                       _irqh_low;
				Irq_handler                       _irqh_high;
				Genode::Signal_context_capability _sig_cap[MAX_PINS];
				bool                              _irq_enabled[MAX_PINS];
				Genode::Lock                      _lock;

			public:

				Gpio_bank(Genode::addr_t base, Genode::size_t size,
				          unsigned irq_low, unsigned irq_high)
				: _reg(base, size),
				  _irqh_low(irq_low, this),
				  _irqh_high(irq_high, this) { }

				Gpio_reg* regs() { return &_reg; }

				void irq(int pin, bool enable)
				{
					_reg.write<Gpio_reg::Int_mask>(enable ? 1 : 0, pin);
					_irq_enabled[pin] = enable;
				}

				void sigh(int pin, Genode::Signal_context_capability cap) {
					_sig_cap[pin] = cap; }
		};


		static Gpio_bank _gpio_bank[MAX_BANKS];

		int _gpio_bank_index(int gpio)  { return gpio >> 5;   }
		int _gpio_index(int gpio)       { return gpio & 0x1f; }

		Imx53_driver()
		{
			for (unsigned i = 0; i < MAX_BANKS; ++i) {
				Gpio_reg *regs = _gpio_bank[i].regs();
				for (unsigned j = 0; j < MAX_PINS; j++) {
					regs->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::LOW_LEVEL, j);
					regs->write<Gpio_reg::Int_mask>(0, j);
				}
				regs->write<Gpio_reg::Int_stat>(0xffffffff);
			}
		}

	public:

		static Imx53_driver& factory();


		/******************************
		 **  Gpio::Driver interface  **
		 ******************************/

		void direction(unsigned gpio, bool input)
		{
			if (verbose) PDBG("gpio=%d input=%d", gpio, input);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Dir>(input ? 0 : 1,
			                               _gpio_index(gpio));
		}

		void write(unsigned gpio, bool level)
		{
			if (verbose) PDBG("gpio=%d level=%d", gpio, level);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();

			gpio_reg->write<Gpio_reg::Data>(level ? 1 : 0,
			                                _gpio_index(gpio));
		}

		bool read(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			return gpio_reg->read<Gpio_reg::Pad_stat>(_gpio_index(gpio));
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

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::FAL_EDGE,
			                                    _gpio_index(gpio));
		}

		void rising_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::RIS_EDGE,
			                                    _gpio_index(gpio));
		}

		void high_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::HIGH_LEVEL,
			                                    _gpio_index(gpio));
		}

		void low_detect(unsigned gpio)
		{
			if (verbose) PDBG("gpio=%d", gpio);

			Gpio_reg *gpio_reg = _gpio_bank[_gpio_bank_index(gpio)].regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::LOW_LEVEL,
			                                    _gpio_index(gpio));
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


Imx53_driver::Gpio_bank Imx53_driver::_gpio_bank[Imx53_driver::MAX_BANKS] = {
	{ Genode::Board_base::GPIO1_MMIO_BASE, Genode::Board_base::GPIO1_MMIO_SIZE,
	  Genode::Board_base::GPIO1_IRQL, Genode::Board_base::GPIO1_IRQH },
	{ Genode::Board_base::GPIO2_MMIO_BASE, Genode::Board_base::GPIO2_MMIO_SIZE,
	  Genode::Board_base::GPIO2_IRQL, Genode::Board_base::GPIO2_IRQH },
	{ Genode::Board_base::GPIO3_MMIO_BASE, Genode::Board_base::GPIO3_MMIO_SIZE,
	  Genode::Board_base::GPIO3_IRQL, Genode::Board_base::GPIO3_IRQH },
	{ Genode::Board_base::GPIO4_MMIO_BASE, Genode::Board_base::GPIO4_MMIO_SIZE,
	  Genode::Board_base::GPIO4_IRQL, Genode::Board_base::GPIO4_IRQH },
	{ Genode::Board_base::GPIO5_MMIO_BASE, Genode::Board_base::GPIO5_MMIO_SIZE,
	  Genode::Board_base::GPIO5_IRQL, Genode::Board_base::GPIO5_IRQH },
	{ Genode::Board_base::GPIO6_MMIO_BASE, Genode::Board_base::GPIO6_MMIO_SIZE,
	  Genode::Board_base::GPIO6_IRQL, Genode::Board_base::GPIO6_IRQH },
	{ Genode::Board_base::GPIO7_MMIO_BASE, Genode::Board_base::GPIO7_MMIO_SIZE,
	  Genode::Board_base::GPIO7_IRQL, Genode::Board_base::GPIO7_IRQH }
};


Imx53_driver& Imx53_driver::factory()
{
	static Imx53_driver driver;
	return driver;
}


void Imx53_driver::Gpio_bank::handle_irq()
{
	Genode::Lock::Guard lock_guard(_lock);

	unsigned long status = _reg.read<Gpio_reg::Int_stat>();

	for(unsigned i = 0; i < MAX_PINS; i++) {
		if ((status & (1 << i)) && _irq_enabled[i] &&
			_sig_cap[i].valid())
			Genode::Signal_transmitter(_sig_cap[i]).submit();
	}

	_reg.write<Gpio_reg::Int_stat>(0xffffffff);
}


#endif /* _DRIVER_H_ */
