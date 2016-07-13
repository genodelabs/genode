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

#ifndef _DRIVERS__GPIO__SPEC__IMX53__DRIVER_H_
#define _DRIVERS__GPIO__SPEC__IMX53__DRIVER_H_

/* Genode includes */
#include <drivers/board_base.h>
#include <gpio/driver.h>
#include <irq_session/connection.h>
#include <timer_session/connection.h>
#include <os/server.h>

/* local includes */
#include "gpio.h"


class Imx53_driver : public Gpio::Driver
{
	private:

		enum {
			PIN_SHIFT = 5,
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

				void handle_irq()
				{
					unsigned long status = _reg.read<Gpio_reg::Int_stat>();

					for(unsigned i = 0; i < MAX_PINS; i++) {
						if ((status & (1 << i)) && _irq_enabled[i] &&
								_sig_cap[i].valid())
							Genode::Signal_transmitter(_sig_cap[i]).submit();
					}
				}

			private:

				class Irq_handler
				{
					private:

						Genode::Irq_connection                  _irq;
						Genode::Signal_rpc_member<Irq_handler>  _dispatcher;
						Gpio_bank                              *_bank;

						void _handle(unsigned)
						{
							_bank->handle_irq();
							_irq.ack_irq();
						}


					public:

						Irq_handler(Server::Entrypoint &ep,
						            unsigned irq, Gpio_bank *bank)
						: _irq(irq), _dispatcher(ep, *this, &Irq_handler::_handle),
						  _bank(bank)
						{
							_irq.sigh(_dispatcher);
							_irq.ack_irq();
						}
				};

				Gpio_reg                          _reg;
				Irq_handler                       _irqh_low;
				Irq_handler                       _irqh_high;
				Genode::Signal_context_capability _sig_cap[MAX_PINS];
				bool                              _irq_enabled[MAX_PINS];

			public:

				Gpio_bank(Server::Entrypoint &ep, Genode::addr_t base,
				          Genode::size_t size, unsigned irq_low, unsigned irq_high)
				: _reg(base, size),
				  _irqh_low(ep, irq_low, this),
				  _irqh_high(ep, irq_high, this) { }

				Gpio_reg* regs() { return &_reg; }

				void irq(int pin, bool enable)
				{
					_reg.write<Gpio_reg::Int_mask>(enable ? 1 : 0, pin);
					_irq_enabled[pin] = enable;
				}

				void ack_irq(int pin)
				{
					_reg.write<Gpio_reg::Int_stat>(1, pin);
				}

				void sigh(int pin, Genode::Signal_context_capability cap) {
					_sig_cap[pin] = cap; }
		};

		Server::Entrypoint &_ep;

		Gpio_bank _gpio_bank_0;
		Gpio_bank _gpio_bank_1;
		Gpio_bank _gpio_bank_2;
		Gpio_bank _gpio_bank_3;
		Gpio_bank _gpio_bank_4;
		Gpio_bank _gpio_bank_5;
		Gpio_bank _gpio_bank_6;

		Gpio_bank *_gpio_bank(int gpio)
		{
			switch (gpio >> PIN_SHIFT) {
			case 0:
				return &_gpio_bank_0;
			case 1:
				return &_gpio_bank_1;
			case 2:
				return &_gpio_bank_2;
			case 3:
				return &_gpio_bank_3;
			case 4:
				return &_gpio_bank_4;
			case 5:
				return &_gpio_bank_5;
			case 6:
				return &_gpio_bank_6;
			}

			Genode::error("no Gpio_bank for pin ", gpio, " available");
			return 0;
		}

		int _gpio_index(int gpio)       { return gpio & 0x1f; }

		Imx53_driver(Server::Entrypoint &ep)
		:
			_ep(ep),
			_gpio_bank_0(_ep, Genode::Board_base::GPIO1_MMIO_BASE, Genode::Board_base::GPIO1_MMIO_SIZE,
			             Genode::Board_base::GPIO1_IRQL, Genode::Board_base::GPIO1_IRQH),
			_gpio_bank_1(_ep, Genode::Board_base::GPIO2_MMIO_BASE, Genode::Board_base::GPIO2_MMIO_SIZE,
			             Genode::Board_base::GPIO2_IRQL, Genode::Board_base::GPIO2_IRQH),
			_gpio_bank_2(_ep, Genode::Board_base::GPIO3_MMIO_BASE, Genode::Board_base::GPIO3_MMIO_SIZE,
			             Genode::Board_base::GPIO3_IRQL, Genode::Board_base::GPIO3_IRQH),
			_gpio_bank_3(_ep, Genode::Board_base::GPIO4_MMIO_BASE, Genode::Board_base::GPIO4_MMIO_SIZE,
			             Genode::Board_base::GPIO4_IRQL, Genode::Board_base::GPIO4_IRQH),
			_gpio_bank_4(_ep, Genode::Board_base::GPIO5_MMIO_BASE, Genode::Board_base::GPIO5_MMIO_SIZE,
			             Genode::Board_base::GPIO5_IRQL, Genode::Board_base::GPIO5_IRQH),
			_gpio_bank_5(_ep, Genode::Board_base::GPIO6_MMIO_BASE, Genode::Board_base::GPIO6_MMIO_SIZE,
			             Genode::Board_base::GPIO6_IRQL, Genode::Board_base::GPIO6_IRQH),
			_gpio_bank_6(_ep, Genode::Board_base::GPIO7_MMIO_BASE, Genode::Board_base::GPIO7_MMIO_SIZE,
			             Genode::Board_base::GPIO7_IRQL, Genode::Board_base::GPIO7_IRQH)
		{
			for (unsigned i = 0; i < MAX_BANKS; ++i) {
				Gpio_reg *regs = _gpio_bank(i << PIN_SHIFT)->regs();
				for (unsigned j = 0; j < MAX_PINS; j++) {
					regs->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::LOW_LEVEL, j);
					regs->write<Gpio_reg::Int_mask>(0, j);
				}
				regs->write<Gpio_reg::Int_stat>(0xffffffff);
			}
		}


	public:

		static Imx53_driver &factory(Server::Entrypoint &ep);

		/******************************
		 **  Gpio::Driver interface  **
		 ******************************/

		void direction(unsigned gpio, bool input)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();
			gpio_reg->write<Gpio_reg::Dir>(input ? 0 : 1,
			                               _gpio_index(gpio));
		}

		void write(unsigned gpio, bool level)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();

			gpio_reg->write<Gpio_reg::Data>(level ? 1 : 0,
			                                _gpio_index(gpio));
		}

		bool read(unsigned gpio)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();
			return gpio_reg->read<Gpio_reg::Pad_stat>(_gpio_index(gpio));
		}

		void debounce_enable(unsigned gpio, bool enable)
		{
			Genode::warning("debounce enable not supported");
		}

		void debounce_time(unsigned gpio, unsigned long us)
		{
			Genode::warning("debounce time not supported");
		}

		void falling_detect(unsigned gpio)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::FAL_EDGE,
			                                    _gpio_index(gpio));
		}

		void rising_detect(unsigned gpio)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::RIS_EDGE,
			                                    _gpio_index(gpio));
		}

		void high_detect(unsigned gpio)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::HIGH_LEVEL,
			                                    _gpio_index(gpio));
		}

		void low_detect(unsigned gpio)
		{
			Gpio_reg *gpio_reg = _gpio_bank(gpio)->regs();
			gpio_reg->write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::LOW_LEVEL,
			                                    _gpio_index(gpio));
		}

		void irq_enable(unsigned gpio, bool enable)
		{
			_gpio_bank(gpio)->irq(_gpio_index(gpio), enable);
		}

		void ack_irq(unsigned gpio)
		{
			_gpio_bank(gpio)->ack_irq(_gpio_index(gpio));
		}

		void register_signal(unsigned gpio,
		                     Genode::Signal_context_capability cap)
		{
			_gpio_bank(gpio)->sigh(_gpio_index(gpio), cap); }

		void unregister_signal(unsigned gpio)
		{
			Genode::Signal_context_capability cap;
			_gpio_bank(gpio)->sigh(_gpio_index(gpio), cap);
		}

		bool gpio_valid(unsigned gpio) { return gpio < (MAX_PINS*MAX_BANKS); }
};


Imx53_driver &Imx53_driver::factory(Server::Entrypoint &ep)
{
	static Imx53_driver driver(ep);
	return driver;
}

#endif /* _DRIVERS__GPIO__SPEC__IMX53__DRIVER_H_ */
