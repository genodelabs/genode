/*
 * \brief  Gpio driver for the i.MX53
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-12-04
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <os/attached_io_mem_dataspace.h>
#include <timer_session/connection.h>
#include <util/mmio.h>
#include <base/signal.h>

/* local includes */
#include "gpio.h"


namespace Gpio {

	using namespace Genode;
	class Driver;
}

static int verbose = 0;

class Gpio::Driver
{
	public:
		enum {
			GPIO1L_IRQ       = 50,
			GPIO1H_IRQ       = 51,
			GPIO2L_IRQ       = 52,
			GPIO2H_IRQ       = 53,
			GPIO3L_IRQ       = 54,
			GPIO3H_IRQ       = 55,
			GPIO4L_IRQ       = 56,
			GPIO4H_IRQ       = 57,
			GPIO5L_IRQ       = 103,
			GPIO5H_IRQ       = 104,
			GPIO6L_IRQ       = 105,
			GPIO6H_IRQ       = 106,
			GPIO7L_IRQ       = 107,
			GPIO7H_IRQ       = 108,
		};

	private:

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			/**
			 * Implementation of 'Delayer' interface
			 */
			void usleep(unsigned us)
			{
				/* polling */
				if (us == 0)
					return;

				unsigned ms = us / 1000;
				if (ms == 0)
					ms = 1;

				Timer::Connection::msleep(ms);
			}
		} _delayer;

		/* memory map */
		enum {
			GPIO1_MMIO_BASE = 0x53f84000,
			GPIO1_MMIO_SIZE = 0x4000,

			GPIO2_MMIO_BASE = 0x53f88000,
			GPIO2_MMIO_SIZE = 0x4000,

			GPIO3_MMIO_BASE = 0x53f8c000,
			GPIO3_MMIO_SIZE = 0x4000,

			GPIO4_MMIO_BASE = 0x53f90000,
			GPIO4_MMIO_SIZE = 0x4000,

			GPIO5_MMIO_BASE = 0x53fdc000,
			GPIO5_MMIO_SIZE = 0x4000,

			GPIO6_MMIO_BASE = 0x53fe0000,
			GPIO6_MMIO_SIZE = 0x4000,

			GPIO7_MMIO_BASE = 0x53fe4000,
			GPIO7_MMIO_SIZE = 0x4000,

			NR_GPIOS        = 7,
			MAX_GPIOS       = 224,
		};


		Attached_io_mem_dataspace _gpio1_mmio;
		Gpio_reg                  _gpio1;
		Attached_io_mem_dataspace _gpio2_mmio;
		Gpio_reg                  _gpio2;
		Attached_io_mem_dataspace _gpio3_mmio;
		Gpio_reg                  _gpio3;
		Attached_io_mem_dataspace _gpio4_mmio;
		Gpio_reg                  _gpio4;
		Attached_io_mem_dataspace _gpio5_mmio;
		Gpio_reg                  _gpio5;
		Attached_io_mem_dataspace _gpio6_mmio;
		Gpio_reg                  _gpio6;
		Attached_io_mem_dataspace _gpio7_mmio;
		Gpio_reg                  _gpio7;

		Gpio_reg*                 _gpio_bank[NR_GPIOS];

		bool                      _irq_enabled[MAX_GPIOS];

		Signal_context_capability _sign[MAX_GPIOS];

		struct Debounce_stat
		{
			unsigned int us;
			bool         enable;

			Debounce_stat() : us(0), enable(false) { }

		} _debounce_stat[MAX_GPIOS];

	public:

		Driver();

		bool set_gpio_direction(int gpio, bool is_input);
		bool set_gpio_dataout(int gpio, bool enable);
		int  get_gpio_datain(int gpio);
		bool set_gpio_debounce_enable(int gpio, bool enable);
		bool set_gpio_debouncing_time(int gpio, unsigned int us);
		bool set_gpio_falling_detect(int gpio, bool enable);
		bool set_gpio_rising_detect(int gpio, bool enable);
		bool set_gpio_irq_enable(int gpio, bool enable);

		void register_signal(Signal_context_capability cap, int gpio)
		{
			if (!_sign[gpio].valid()) {
				_sign[gpio] = cap;
			}
		}

		void handle_event(int irq_number);

	private:
		Gpio_reg *_get_gpio_bank(int gpio)  { return _gpio_bank[gpio >> 5]; }
		bool      _gpio_valid(int gpio)     { return (gpio < MAX_GPIOS) ? true : false; }
		int       _get_gpio_index(int gpio) { return gpio & 0x1f; }

		inline void _irq_signal_send(int gpio)
		{
			if (_sign[gpio].valid()) {
				if (verbose)
					PDBG("gpio=%d", gpio);

				Signal_transmitter transmitter(_sign[gpio]);
				transmitter.submit();
			}
		}

		inline void _irq_event(int gpio_bank, uint32_t status)
		{
			for (int i=0; i<32; i++) {
				if ((status & (1 << i)) && _irq_enabled[(gpio_bank<<5) + i])
					_irq_signal_send( (gpio_bank<<5) + i );
			}
		}

};


Gpio::Driver::Driver()
:
	_gpio1_mmio(GPIO1_MMIO_BASE, GPIO1_MMIO_SIZE),
	_gpio1((addr_t)_gpio1_mmio.local_addr<void>()),
	_gpio2_mmio(GPIO2_MMIO_BASE, GPIO2_MMIO_SIZE),
	_gpio2((addr_t)_gpio2_mmio.local_addr<void>()),
	_gpio3_mmio(GPIO3_MMIO_BASE, GPIO3_MMIO_SIZE),
	_gpio3((addr_t)_gpio3_mmio.local_addr<void>()),
	_gpio4_mmio(GPIO4_MMIO_BASE, GPIO4_MMIO_SIZE),
	_gpio4((addr_t)_gpio4_mmio.local_addr<void>()),
	_gpio5_mmio(GPIO5_MMIO_BASE, GPIO5_MMIO_SIZE),
	_gpio5((addr_t)_gpio5_mmio.local_addr<void>()),
	_gpio6_mmio(GPIO6_MMIO_BASE, GPIO6_MMIO_SIZE),
	_gpio6((addr_t)_gpio6_mmio.local_addr<void>()),
	_gpio7_mmio(GPIO7_MMIO_BASE, GPIO7_MMIO_SIZE),
	_gpio7((addr_t)_gpio7_mmio.local_addr<void>())

{
	_gpio_bank[0] = &_gpio1;
	_gpio_bank[1] = &_gpio2;
	_gpio_bank[2] = &_gpio3;
	_gpio_bank[3] = &_gpio4;
	_gpio_bank[4] = &_gpio5;
	_gpio_bank[5] = &_gpio6;
	_gpio_bank[6] = &_gpio7;

	for (int i = 0; i < MAX_GPIOS; ++i) {
		Gpio_reg *gpio_reg = _get_gpio_bank(i);
		gpio_reg->write<Gpio_reg::Int_conf::Pin>(Gpio_reg::Int_conf::HIGH_LEVEL,
		                                         _get_gpio_index(i));
	}
}

bool Gpio::Driver::set_gpio_direction(int gpio, bool is_input)
{
	if (verbose) {
		PDBG("gpio=%d is_input=%d", gpio, is_input);
	}

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	if (is_input)
		gpio_reg->write<Gpio_reg::Dir::Pin>(0, _get_gpio_index(gpio));
	else
		gpio_reg->write<Gpio_reg::Dir::Pin>(1, _get_gpio_index(gpio));

	return true;
}

bool Gpio::Driver::set_gpio_dataout(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	if (enable)
		gpio_reg->write<Gpio_reg::Data::Pin>(1, _get_gpio_index(gpio));
	else
		gpio_reg->write<Gpio_reg::Data::Pin>(0, _get_gpio_index(gpio));

	return true;
}

int Gpio::Driver::get_gpio_datain(int gpio)
{
	if (verbose)
		PDBG("gpio=%d", gpio);

	if (!_gpio_valid(gpio))
		return -1;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	if (_debounce_stat[gpio].enable)
		_delayer.usleep(_debounce_stat[gpio].us);

	return gpio_reg->read<Gpio_reg::Pad_stat::Pin>(_get_gpio_index(gpio));
}

bool Gpio::Driver::set_gpio_debounce_enable(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	_debounce_stat[gpio].enable = enable;

	return true;
}

bool Gpio::Driver::set_gpio_debouncing_time(int gpio, unsigned int us)
{
	if (verbose)
		PDBG("gpio=%d us=%d", gpio, us);

	if (!_gpio_valid(gpio))
		return false;

	_debounce_stat[gpio].us = us;

	return true;
}

bool Gpio::Driver::set_gpio_falling_detect(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	if (enable)
		gpio_reg->write<Gpio_reg::Int_conf::Pin>(Gpio_reg::Int_conf::FAL_EDGE,
		                                         _get_gpio_index(gpio));
	else
		gpio_reg->write<Gpio_reg::Int_conf::Pin>(Gpio_reg::Int_conf::HIGH_LEVEL,
		                                         _get_gpio_index(gpio));

	return true;
}

bool Gpio::Driver::set_gpio_rising_detect(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	if (enable)
		gpio_reg->write<Gpio_reg::Int_conf::Pin>(Gpio_reg::Int_conf::RIS_EDGE,
		                                         _get_gpio_index(gpio));
	else
		gpio_reg->write<Gpio_reg::Int_conf::Pin>(Gpio_reg::Int_conf::HIGH_LEVEL,
		                                         _get_gpio_index(gpio));

	return true;
}

bool Gpio::Driver::set_gpio_irq_enable(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	if (enable)
		gpio_reg->write<Gpio_reg::Int_mask::Pin>(1, _get_gpio_index(gpio));
	else
		gpio_reg->write<Gpio_reg::Int_mask::Pin>(0, _get_gpio_index(gpio));

	_irq_enabled[gpio] = enable;

	return true;
}

void Gpio::Driver::handle_event(int irq_number)
{
	if (verbose)
		PDBG("IRQ #%d\n", irq_number);

	int gpio_bank = 0;

	switch (irq_number) {
	case GPIO1L_IRQ:
	case GPIO1H_IRQ:
		gpio_bank = 0;
		break;
	case GPIO2L_IRQ:
	case GPIO2H_IRQ:
		gpio_bank = 1;
		break;
	case GPIO3L_IRQ:
	case GPIO3H_IRQ:
		gpio_bank = 2;
		break;
	case GPIO4L_IRQ:
	case GPIO4H_IRQ:
		gpio_bank = 3;
		break;
	case GPIO5L_IRQ:
	case GPIO5H_IRQ:
		gpio_bank = 4;
		break;
	case GPIO6L_IRQ:
	case GPIO6H_IRQ:
		gpio_bank = 5;
		break;
	case GPIO7L_IRQ:
	case GPIO7H_IRQ:
		gpio_bank = 6;
		break;
	default:
		PERR("Unknown Irq number!\n");
		return;
	}

	int stat = _gpio_bank[gpio_bank]->read<Gpio_reg::Int_stat>();

	if (verbose)
		PDBG("GPIO1 IRQSTATUS=%08x\n", stat);

	_irq_event(gpio_bank, stat);
	_gpio_bank[gpio_bank]->write<Gpio_reg::Int_stat>(0xffffffff);
}

#endif /* _DRIVER_H_ */
