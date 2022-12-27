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

#ifndef _DRIVERS__GPIO__IMX__DRIVER_H_
#define _DRIVERS__GPIO__IMX__DRIVER_H_

/* Genode includes */
#include <gpio/driver.h>
#include <irq_session/client.h>
#include <platform_session/device.h>
#include <timer_session/connection.h>

/* local includes */
#include <gpio.h>


namespace Platform { struct Device_client; }


/*
 * Transitionally wrapper for accessing platform devices used until
 * the migration to Platform::Device::Mmio API is completed.
 */
struct Platform::Device_client : Rpc_client<Device_interface>
{
	Device_client(Capability<Device_interface> cap)
	: Rpc_client<Device_interface>(cap) { }

	Irq_session_capability irq(unsigned id = 0)
	{
		return call<Rpc_irq>(id);
	}

	Io_mem_session_capability io_mem(unsigned id, Range &range)
	{
		return call<Rpc_io_mem>(id, range);
	}

	Dataspace_capability io_mem_dataspace(unsigned id = 0)
	{
		Range range { };
		return Io_mem_session_client(io_mem(id, range)).dataspace();
	}
};


class Imx_driver : public Gpio::Driver
{
	using Pin = Gpio::Pin;
	private:

		enum {
			PIN_SHIFT = 5,
			MAX_BANKS = 8,
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

						Genode::Irq_session_client              _irq;
						Genode::Io_signal_handler<Irq_handler>  _dispatcher;
						Gpio_bank                              *_bank;

						void _handle()
						{
							_bank->handle_irq();
							_irq.ack_irq();
						}

					public:

						Irq_handler(Genode::Env &env,
						            Genode::Irq_session_capability cap, Gpio_bank *bank)
						: _irq(cap), _dispatcher(env.ep(), *this, &Irq_handler::_handle),
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

				Gpio_bank(Genode::Env &env, Genode::Dataspace_capability io_mem,
				          Genode::Irq_session_capability irq_low,
				          Genode::Irq_session_capability irq_high)
				: _reg(env, io_mem),
				  _irqh_low(env, irq_low, this),
				  _irqh_high(env, irq_high, this) { }

				Gpio_reg& regs() { return _reg; }

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

		Platform::Connection             _platform_connection;
		Genode::Constructible<Gpio_bank> _gpio_banks[MAX_BANKS];

		template <typename FN>
		void _with_gpio(Pin gpio, FN const &fn)
		{
			unsigned bank = gpio.value >> PIN_SHIFT;

			if (bank >= MAX_BANKS) {
				Genode::warning("no Gpio_bank for pin ", gpio, " ignoring");
				return;
			}

			fn(*_gpio_banks[bank]);
		}

		unsigned _gpio_index(Pin gpio) const { return gpio.value & 0x1f; }

	public:

		Imx_driver(Genode::Env &env)
		: _platform_connection(env)
		{
			using namespace Platform;

			unsigned i = 0;

			_platform_connection.with_xml([&] (Xml_node &xml) {
				xml.for_each_sub_node("device", [&] (Xml_node node) {
					if (i >= MAX_BANKS) return;

					Device::Name name = node.attribute_value("name", Device::Name());
					if (name == Device::Name()) return;

					Device_client device {  _platform_connection.acquire_device(name.string()) };

					Genode::Dataspace_capability io_mem  = device.io_mem_dataspace();
					if (io_mem.valid() == false) {
						Genode::warning("No 'io_mem' node present for device '", name, "' skipping");
						return;
					}

					Genode::Irq_session_capability irq_low = device.irq(0);
					if (irq_low.valid() == false) {
						Genode::warning("No 'irq' node for low present for device '", name, "' skipping");
						return;
					}

					Genode::Irq_session_capability irq_high = device.irq(1);
					if (irq_high.valid() == false) {
						Genode::warning("No 'irq' node for high present for device '", name, "' skipping");
						return;
					}

					_gpio_banks[i].construct(env, io_mem, irq_low, irq_high);

					Gpio_reg &regs = _gpio_banks[i]->regs();
					for (unsigned j = 0; j < MAX_PINS; j++) {
						regs.write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::LOW_LEVEL, j);
						regs.write<Gpio_reg::Int_mask>(0, j);
					}
					regs.write<Gpio_reg::Int_stat>(0xffffffff);

					i++;
				});
			});
		}

		/******************************
		 **  Gpio::Driver interface  **
		 ******************************/

		void direction(Pin gpio, bool input) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				gpio_reg.write<Gpio_reg::Dir>(input ? 0 : 1,
				                               _gpio_index(gpio));
			});
		}

		void write(Pin gpio, bool level) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				gpio_reg.write<Gpio_reg::Data>(level ? 1 : 0,
				                               _gpio_index(gpio));
			});
		}

		bool read(Pin gpio) override
		{
			bool ret = false;

			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				ret = gpio_reg.read<Gpio_reg::Pad_stat>(_gpio_index(gpio));
			});

			return ret;
		}

		void debounce_enable(Pin /* gpio */, bool /* enable */) override
		{
			Genode::warning("debounce enable not supported");
		}

		void debounce_time(Pin /* gpio */, unsigned long /* us */) override
		{
			Genode::warning("debounce time not supported");
		}

		void falling_detect(Pin gpio) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				gpio_reg.write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::FAL_EDGE,
				                                   _gpio_index(gpio));
			});
		}

		void rising_detect(Pin gpio) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				gpio_reg.write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::RIS_EDGE,
				                                   _gpio_index(gpio));
			});
		}

		void high_detect(Pin gpio) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				gpio_reg.write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::HIGH_LEVEL,
				                                   _gpio_index(gpio));
			});
		}

		void low_detect(Pin gpio) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				Gpio_reg &gpio_reg = bank.regs();
				gpio_reg.write<Gpio_reg::Int_conf>(Gpio_reg::Int_conf::LOW_LEVEL,
				                                    _gpio_index(gpio));
			});
		}

		void irq_enable(Pin gpio, bool enable) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				bank.irq(_gpio_index(gpio), enable);
			});
		}

		void ack_irq(Pin gpio) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				bank.ack_irq(_gpio_index(gpio));
			});
		}

		void register_signal(Pin gpio,
		                     Genode::Signal_context_capability cap) override
		{
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				bank.sigh(_gpio_index(gpio), cap);
			});
		}

		void unregister_signal(Pin gpio) override
		{
			Genode::Signal_context_capability cap;
			_with_gpio(gpio, [&](Gpio_bank &bank) {
				bank.sigh(_gpio_index(gpio), cap);
			});
		}

		bool gpio_valid(Pin gpio) override { return gpio.value < (MAX_PINS*MAX_BANKS); }
};

#endif /* _DRIVERS__GPIO__IMX__DRIVER_H_ */
