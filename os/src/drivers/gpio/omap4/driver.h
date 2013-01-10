/*
 * \brief  Gpio driver for the OMAP4
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
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
			GPIO1_IRQ       = 29 + 32,
			GPIO2_IRQ       = 30 + 32,
			GPIO3_IRQ       = 31 + 32,
			GPIO4_IRQ       = 32 + 32,
			GPIO5_IRQ       = 33 + 32,
			GPIO6_IRQ       = 34 + 32,
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
			GPIO1_MMIO_BASE = 0x4a310000,
			GPIO1_MMIO_SIZE = 0x1000,

			GPIO2_MMIO_BASE = 0x48055000,
			GPIO2_MMIO_SIZE = 0x1000,

			GPIO3_MMIO_BASE = 0x48057000,
			GPIO3_MMIO_SIZE = 0x1000,

			GPIO4_MMIO_BASE = 0x48059000,
			GPIO4_MMIO_SIZE = 0x1000,

			GPIO5_MMIO_BASE = 0x4805b000,
			GPIO5_MMIO_SIZE = 0x1000,

			GPIO6_MMIO_BASE = 0x4805d000,
			GPIO6_MMIO_SIZE = 0x1000,

			NR_GPIOS        = 6,
			MAX_GPIOS       = 192,
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

		Gpio_reg                  *_gpio_bank[NR_GPIOS];

		bool                      irq_enabled[MAX_GPIOS];

		Signal_context_capability _sign[MAX_GPIOS];

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
			if (!_sign[gpio].valid())
			{
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
			if (_sign[gpio].valid())
			{
				if (verbose)
					PDBG("gpio=%d", gpio);
				
				Signal_transmitter transmitter(_sign[gpio]);
				transmitter.submit();
			}
		}

		inline void _irq_event(int gpio_bank, uint32_t status)
		{
			for(int i=0; i<32; i++)
			{
				if ( (status & (1 << i)) && irq_enabled[(gpio_bank<<5) + i] )
					_irq_signal_send( (gpio_bank<<5) + i );
			}
		}

		void _handle_event_gpio1();
		void _handle_event_gpio2();
		void _handle_event_gpio3();
		void _handle_event_gpio4();
		void _handle_event_gpio5();
		void _handle_event_gpio6();

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
	_gpio6((addr_t)_gpio6_mmio.local_addr<void>())
{
	_gpio_bank[0] = &_gpio1;
	_gpio_bank[1] = &_gpio2;
	_gpio_bank[2] = &_gpio3;
	_gpio_bank[3] = &_gpio4;
	_gpio_bank[4] = &_gpio5;
	_gpio_bank[5] = &_gpio6;

	for (int i = 0; i < NR_GPIOS; ++i)
	{
		uint32_t r = _gpio_bank[i]->read<Gpio_reg::Ctrl>();
		if (verbose)
			PDBG("GPIO%d ctrl=%08x", i+1, r);
	}
}


bool Gpio::Driver::set_gpio_direction(int gpio, bool is_input)
{
	if (verbose)
		PDBG("gpio=%d is_input=%d", gpio, is_input);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	uint32_t value = gpio_reg->read<Gpio_reg::Oe>();
	if (is_input)
		value |= (1 << _get_gpio_index(gpio));
	else
		value &= ~(1 << _get_gpio_index(gpio));
	gpio_reg->write<Gpio_reg::Oe>(value);

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
		gpio_reg->write<Gpio_reg::Setdataout>(1 << _get_gpio_index(gpio));
	else
		gpio_reg->write<Gpio_reg::Cleardataout>(1 << _get_gpio_index(gpio));

	return true;
}


int Gpio::Driver::get_gpio_datain(int gpio)
{
	if (verbose)
		PDBG("gpio=%d", gpio);

	if (!_gpio_valid(gpio))
		return -1;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	uint32_t value = gpio_reg->read<Gpio_reg::Datain>();

	return (value & (1 << _get_gpio_index(gpio))) != 0 ;
}


bool Gpio::Driver::set_gpio_debounce_enable(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	uint32_t value = gpio_reg->read<Gpio_reg::Debounceenable>();
	if (enable)
		value |= (1 << _get_gpio_index(gpio));
	else
		value &= ~(1 << _get_gpio_index(gpio));
	gpio_reg->write<Gpio_reg::Debounceenable>(value);

	return true;
}


bool Gpio::Driver::set_gpio_debouncing_time(int gpio, unsigned int us)
{
	if (verbose)
		PDBG("gpio=%d us=%d", gpio, us);

	if (!_gpio_valid(gpio))
		return false;

	unsigned char debounce;

	if (us < 32)
		debounce = 0x01;
	else if (us > 7936)
		debounce = 0xff;
	else
		debounce = (us / 0x1f) - 1;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	gpio_reg->write<Gpio_reg::Debouncingtime::Time>(debounce);

	return true;
}


bool Gpio::Driver::set_gpio_falling_detect(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	uint32_t value = gpio_reg->read<Gpio_reg::Fallingdetect>();
	if (enable)
		value |= (1 << _get_gpio_index(gpio));
	else
		value &= ~(1 << _get_gpio_index(gpio));
	gpio_reg->write<Gpio_reg::Fallingdetect>(value);

	return true;
}


bool Gpio::Driver::set_gpio_rising_detect(int gpio, bool enable)
{
	if (verbose)
		PDBG("gpio=%d enable=%d", gpio, enable);

	if (!_gpio_valid(gpio))
		return false;

	Gpio_reg *gpio_reg = _get_gpio_bank(gpio);

	uint32_t value = gpio_reg->read<Gpio_reg::Risingdetect>();
	if (enable)
		value |= (1 << _get_gpio_index(gpio));
	else
		value &= ~(1 << _get_gpio_index(gpio));
	gpio_reg->write<Gpio_reg::Risingdetect>(value);

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
	{
		gpio_reg->write<Gpio_reg::Irqstatus_0>(1 << _get_gpio_index(gpio));
		gpio_reg->write<Gpio_reg::Irqstatus_set_0>(1 << _get_gpio_index(gpio));
		irq_enabled[gpio] = true;
	}
	else
	{
		gpio_reg->write<Gpio_reg::Irqstatus_clr_0>(1 << _get_gpio_index(gpio));
		irq_enabled[gpio] = false;
	}

	return true;
}


void Gpio::Driver::handle_event(int irq_number)
{
	if (verbose)
		PDBG("IRQ #%d\n", irq_number-32);
	switch(irq_number)
	{
		case GPIO1_IRQ: _handle_event_gpio1(); break;
		case GPIO2_IRQ: _handle_event_gpio2(); break;
		case GPIO3_IRQ: _handle_event_gpio3(); break;
		case GPIO4_IRQ: _handle_event_gpio4(); break;
		case GPIO5_IRQ: _handle_event_gpio5(); break;
		case GPIO6_IRQ: _handle_event_gpio6(); break;
	}
}


void Gpio::Driver::_handle_event_gpio1()
{
	int sts = _gpio1.read<Gpio_reg::Irqstatus_0>();
	if (verbose)
		PDBG("GPIO1 IRQSTATUS=%08x\n", sts);
	_irq_event(0, sts);
	_gpio1.write<Gpio_reg::Irqstatus_0>(0xffffffff);
}


void Gpio::Driver::_handle_event_gpio2()
{
	int sts = _gpio2.read<Gpio_reg::Irqstatus_0>();
	if (verbose)
		PDBG("GPIO2 IRQSTATUS=%08x\n", sts);
	_irq_event(1, sts);
	_gpio2.write<Gpio_reg::Irqstatus_0>(0xffffffff);
}


void Gpio::Driver::_handle_event_gpio3()
{
	int sts = _gpio3.read<Gpio_reg::Irqstatus_0>();
	if (verbose)
		PDBG("GPIO3 IRQSTATUS=%08x\n", sts);
	_irq_event(2, sts);
	_gpio3.write<Gpio_reg::Irqstatus_0>(0xffffffff);
}


void Gpio::Driver::_handle_event_gpio4()
{
	int sts = _gpio4.read<Gpio_reg::Irqstatus_0>();
	if (verbose)
		PDBG("GPIO4 IRQSTATUS=%08x\n", sts);
	_irq_event(3, sts);
	_gpio4.write<Gpio_reg::Irqstatus_0>(0xffffffff);
}


void Gpio::Driver::_handle_event_gpio5()
{
	int sts = _gpio5.read<Gpio_reg::Irqstatus_0>();
	if (verbose)
		PDBG("GPIO5 IRQSTATUS=%08x\n", sts);
	_irq_event(4, sts);
	_gpio5.write<Gpio_reg::Irqstatus_0>(0xffffffff);
}


void Gpio::Driver::_handle_event_gpio6()
{
	int sts = _gpio6.read<Gpio_reg::Irqstatus_0>();
	if (verbose)
		PDBG("GPIO6 IRQSTATUS=%08x\n", sts);
	_irq_event(5, sts);
	_gpio6.write<Gpio_reg::Irqstatus_0>(0xffffffff);
}

#endif /* _DRIVER_H_ */
