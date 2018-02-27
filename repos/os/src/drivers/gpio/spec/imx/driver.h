/*
 * \brief  Gpio driver for Freescale
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-12-04
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__GPIO__SPEC__IMX__DRIVER_H_
#define _DRIVERS__GPIO__SPEC__IMX__DRIVER_H_

/* Genode includes */
#include <gpio/driver.h>
#include <irq_session/connection.h>
#include <timer_session/connection.h>
#include <os/server.h>

/* local includes */
#include <board.h>
#include <gpio.h>


class Imx_driver : public Gpio::Driver
{
	private:

		enum {
			PIN_SHIFT = 5,
			MAX_BANKS = 7,
			MAX_PINS  = 32
		};


		class Gpio_bank
		{
			public:

				void handle_irq()
				{
					unsigned long status = _reg.read<Gpio_reg::Int_stat>();

					for(unsigned i = 0; i < MAX_PINS; i++) {
						if ((status & (1 << i)) && _irq_enabled[i] &&
								_sig_cap[i].valid()) {
							Genode::Signal_transmitter(_sig_cap[i]).submit();
							_reg.write<Gpio_reg::Int_mask>(0, i);
						}
					}
				}

			private:

				class Irq_handler
				{
					private:

						/*
						 * Noncopyable
						 */
						Irq_handler(Irq_handler const &);
						Irq_handler &operator = (Irq_handler const &);

						Genode::Irq_connection                  _irq;
						Genode::Signal_handler<Irq_handler>     _dispatcher;
						Gpio_bank                              *_bank;

						void _handle()
						{
							_bank->handle_irq();
							_irq.ack_irq();
						}

					public:

						Irq_handler(Genode::Env &env,
						            unsigned irq, Gpio_bank *bank)
						: _irq(env, irq), _dispatcher(env.ep(), *this, &Irq_handler::_handle),
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

				Gpio_bank(Genode::Env &env, Genode::addr_t base,
				          Genode::size_t size, unsigned irq_low, unsigned irq_high)
				: _reg(env, base, size),
				  _irqh_low(env, irq_low, this),
				  _irqh_high(env, irq_high, this) { }

				Gpio_reg* regs() { return &_reg; }

				void irq(int pin, bool enable)
				{
					_reg.write<Gpio_reg::Int_mask>(enable ? 1 : 0, pin);
					_irq_enabled[pin] = enable;
				}

				void ack_irq(int pin)
				{
					_reg.write<Gpio_reg::Int_stat>(1, pin);
					if (_irq_enabled[pin]) _reg.write<Gpio_reg::Int_mask>(1, pin);
				}

				void sigh(int pin, Genode::Signal_context_capability cap) {
					_sig_cap[pin] = cap; }
		};

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

		Imx_driver(Genode::Env &env)
		:
			_gpio_bank_0(env, Board::GPIO1_MMIO_BASE, Board::GPIO1_MMIO_SIZE,
			             Board::GPIO1_IRQL, Board::GPIO1_IRQH),
			_gpio_bank_1(env, Board::GPIO2_MMIO_BASE, Board::GPIO2_MMIO_SIZE,
			             Board::GPIO2_IRQL, Board::GPIO2_IRQH),
			_gpio_bank_2(env, Board::GPIO3_MMIO_BASE, Board::GPIO3_MMIO_SIZE,
			             Board::GPIO3_IRQL, Board::GPIO3_IRQH),
			_gpio_bank_3(env, Board::GPIO4_MMIO_BASE, Board::GPIO4_MMIO_SIZE,
			             Board::GPIO4_IRQL, Board::GPIO4_IRQH),
			_gpio_bank_4(env, Board::GPIO5_MMIO_BASE, Board::GPIO5_MMIO_SIZE,
			             Board::GPIO5_IRQL, Board::GPIO5_IRQH),
			_gpio_bank_5(env, Board::GPIO6_MMIO_BASE, Board::GPIO6_MMIO_SIZE,
			             Board::GPIO6_IRQL, Board::GPIO6_IRQH),
			_gpio_bank_6(env, Board::GPIO7_MMIO_BASE, Board::GPIO7_MMIO_SIZE,
			             Board::GPIO7_IRQL, Board::GPIO7_IRQH)
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

		static Imx_driver &factory(Genode::Env &env);

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

		void debounce_enable(unsigned /* gpio */, bool /* enable */)
		{
			Genode::warning("debounce enable not supported");
		}

		void debounce_time(unsigned /* gpio */, unsigned long /* us */)
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

#endif /* _DRIVERS__GPIO__SPEC__IMX53__DRIVER_H_ */
