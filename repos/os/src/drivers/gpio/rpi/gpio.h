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

#ifndef _GPIO_H_
#define _GPIO_H_

/* Genode includes */
#include <base/printf.h>
#include <os/attached_io_mem_dataspace.h>
#include <util/mmio.h>
#include <timer_session/connection.h>

class Gpio_reg : Genode::Attached_io_mem_dataspace , Genode::Mmio
{
	/**
	 * GPIO Function Select Registers
	 */
	struct gpfsel0 : Register <0x00,32> {};
	struct gpfsel1 : Register <0x04,32> {};
	struct gpfsel2 : Register <0x08,32> {};
	struct gpfsel3 : Register <0x0C,32> {};
	struct gpfsel4 : Register <0x10,32> {};
	struct gpfsel5 : Register <0x14,32> {};

	/**
	 * GPIO Pin Output Set Registers
	 */
	struct gpset0 : Register <0x1C,32> {};
	struct gpset1 : Register <0x20,32> {};

	/**
	 * GPIO Pin Output Clear Registers
	 */
	struct gpclr0 : Register <0x28,32> {};
	struct gpclr1 : Register <0x2C,32> {};

	/**
	 * GPIO Pin Level Registers
	 */
	struct gplev0 : Register <0x34,32> {};
	struct gplev1 : Register <0x38,32> {};

	/**
	 * GPIO Pin Event Detect Status Registers
	 */
	struct gppeds0 : Register <0x40,32> {};
	struct gppeds1 : Register <0x44,32> {};

	/**
	 * GPIO Pin Rising Edge Detect Enable Registers
	 */
	struct gpren0 : Register <0x4C,32> {};
	struct gpren1 : Register <0x50,32> {};

	/**
	 * GPIO Pin Falling Edge Detect Enable Registers
	 */
	struct gpfen0 : Register <0x58,32> {};
	struct gpfen1 : Register <0x5C,32> {};

	/**
	 * GPIO Pin High Detect Enable Registers
	 */
	struct gphen0 : Register <0x64,32> {};
	struct gphen1 : Register <0x68,32> {};

	/**
	 * GPIO Pin Low Detect Enable Registers
	 */
	struct gplen0 : Register <0x70,32> {};
	struct gplen1 : Register <0x74,32> {};

	/**
	 * GPIO Pin Aync. Rising Edge Detect Registers
	 */
	struct gparen0 : Register <0x7C,32> {};
	struct gparen1 : Register <0x80,32> {};

	/**
	 * GPIO Pin Async. Falling Edge Detect Registers
	 */
	struct gpafen0 : Register <0x88,32> {};
	struct gpafen1 : Register <0x8C,32> {};

	/**
	 * GPIO Pin Pull-up/down Enable Registers
	 */
	struct gppud : Register <0x94,32> {};

	/**
	 * GPIO Pin Pull-up/down Enable Clock Registers
	 */
	struct gppudclk0 : Register <0x98,32> {};
	struct gppudclk1 : Register <0x9C,32> {};

	struct Timer_delayer : Timer::Connection, Genode::Mmio::Delayer
	{
		/**
		 * Implementation of 'Delayer' interface
		 */
		void usleep(unsigned us) { Timer::Connection::usleep(us); }
	} _delayer;

public:
	Gpio_reg(Genode::addr_t base, Genode::off_t offset, Genode::size_t size)
		:Genode::Attached_io_mem_dataspace(base, size),
		 Genode::Mmio ((Genode::addr_t)local_addr<Gpio_reg>()+offset ) { }

	enum{
		GPIO_FSEL_INPUT  = 0,
		GPIO_FSEL_OUTPUT = 1,
		GPIO_FSEL_ALT0   = 4,
		GPIO_FSEL_ALT1   = 5,
		GPIO_FSEL_ALT2   = 6,
		GPIO_FSEL_ALT3   = 7,
		GPIO_FSEL_ALT4   = 3,
		GPIO_FSEL_ALT5   = 2
	};
	
	void set_gpio_function(unsigned gpio,unsigned function)
	{

		unsigned int reg_value;
		int gpio_offset;
		int idx;

		/* Set a pull-up internal resistor in the input pin to avoid
		 * electromagnetic radiation or static noise
		 */
		if (function==GPIO_FSEL_INPUT)
		{
			write<gppud>(1);
			_delayer.usleep(1);
			gpio_offset = gpio%32;
			idx = gpio/32;
			switch(idx){
			case 0:
				reg_value = read<gppudclk0>();
				reg_value |= 1 << gpio_offset;
				write<gppudclk0>(reg_value);
				break;
			case 1:
				reg_value = read<gppudclk1>();
				reg_value |= 1 << gpio_offset;
				write<gppudclk1>(reg_value);
				break;
			default:
				PERR("Wrong GPIO pin number: %d.",gpio);
			}
			_delayer.usleep(1);
			write<gppud>(0);
			write<gppudclk0>(0);
		}

		/* Set the pin function */
		gpio_offset = 3*(gpio%10);
		idx = gpio/10;
		switch(idx){
		case 0:
			reg_value = read<gpfsel0>();
			reg_value &= ~(7 << gpio_offset);
			reg_value |= function << gpio_offset;
			write<gpfsel0>(reg_value);
			break;
		case 1:
			reg_value = read<gpfsel1>();
			reg_value &= ~(7 << gpio_offset);
			reg_value |= function << gpio_offset;
			write<gpfsel1>(reg_value);
			break;
		case 2:
			reg_value = read<gpfsel2>();
			reg_value &= ~(7 << gpio_offset);
			reg_value |= function << gpio_offset;
			write<gpfsel2>(reg_value);
			break;
		case 3:
			reg_value = read<gpfsel3>();
			reg_value &= ~(7 << gpio_offset);
			reg_value |= function << gpio_offset;
			write<gpfsel3>(reg_value);
			break;
		case 4:
			reg_value = read<gpfsel4>();
			reg_value &= ~(7 << gpio_offset);
			reg_value |= function << gpio_offset;
			write<gpfsel4>(reg_value);
			break;
		case 5:
			reg_value = read<gpfsel5>();
			reg_value &= ~(7 << gpio_offset);
			reg_value |= function << gpio_offset;
			write<gpfsel5>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	unsigned int get_gpio_function(unsigned gpio)
	{
		int gpio_offset = 3*(gpio%10);
		int idx = gpio/10;
		switch(idx){
		case 0:
			return (read<gpfsel0>() >> gpio_offset) & 7;
		case 1:
			return (read<gpfsel1>() >> gpio_offset) & 7;
		case 2:
			return (read<gpfsel2>() >> gpio_offset) & 7;
		case 3:
			return (read<gpfsel3>() >> gpio_offset) & 7;
		case 4:
			return (read<gpfsel4>() >> gpio_offset) & 7;
		case 5:
			return (read<gpfsel5>() >> gpio_offset) & 7;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
		return 0;
	}

	int get_gpio_level(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		switch(idx){
		case 0:
			return (read<gplev0>() >> gpio_offset) & 0x1;
		case 1:
			return (read<gplev1>() >> gpio_offset) & 0x1;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
		return 0;
	}

	void set_gpio_level(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			reg_value = read<gpset0>();
			reg_value |= (0x1 << gpio_offset);
			write<gpset0>(reg_value);
			break;
		case 1:
			reg_value = read<gpset1>();
			reg_value |= (0x1 << gpio_offset);
			write<gpset1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void clear_gpio_level(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			reg_value = read<gpclr0>();
			reg_value |= (0x1 << gpio_offset);
			write<gpclr0>(reg_value);
			break;
		case 1:
			reg_value = read<gpclr1>();
			reg_value |= (0x1 << gpio_offset);
			write<gpclr1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void set_gpio_falling_detect(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			/* Rising */
			reg_value = read<gpren0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren0>(reg_value);

			/* High */
			reg_value = read<gphen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen0>(reg_value);

			/* Low */
			reg_value = read<gplen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen0>(reg_value);

			/* Async Rising */
			reg_value = read<gparen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen0>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen0>(reg_value);

			/* Falling */
			reg_value = read<gpfen0>();
			reg_value |= (0x1 << gpio_offset);
			write<gpfen0>(reg_value);
			break;
		case 1:
			/* Rising */
			reg_value = read<gpren1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren1>(reg_value);

			/* High */
			reg_value = read<gphen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen1>(reg_value);

			/* Low */
			reg_value = read<gplen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen1>(reg_value);

			/* Async Rising */
			reg_value = read<gparen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen1>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen1>(reg_value);

			/* Falling */
			reg_value = read<gpfen1>();
			reg_value |= (0x1 << gpio_offset);
			write<gpfen1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void set_gpio_rising_detect(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			/* High */
			reg_value = read<gphen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen0>(reg_value);

			/* Low */
			reg_value = read<gplen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen0>(reg_value);

			/* Async Rising */
			reg_value = read<gparen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen0>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen0>(reg_value);

			/* Falling */
			reg_value = read<gpfen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen0>(reg_value);

			/* Rising */
			reg_value = read<gpren0>();
			reg_value |= (0x1 << gpio_offset);
			write<gpren0>(reg_value);
			break;
		case 1:
			/* High */
			reg_value = read<gphen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen1>(reg_value);

			/* Low */
			reg_value = read<gplen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen1>(reg_value);

			/* Async Rising */
			reg_value = read<gparen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen1>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen1>(reg_value);

			/* Falling */
			reg_value = read<gpfen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen1>(reg_value);

			/* Rising */
			reg_value = read<gpren1>();
			reg_value |= (0x1 << gpio_offset);
			write<gpren1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void set_gpio_high_detect(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			/* Rising */
			reg_value = read<gpren0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren0>(reg_value);

			/* Low */
			reg_value = read<gplen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen0>(reg_value);

			/* Async Rising */
			reg_value = read<gparen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen0>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen0>(reg_value);

			/* Falling */
			reg_value = read<gpfen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen0>(reg_value);

			/* High */
			reg_value = read<gphen0>();
			reg_value |= (0x1 << gpio_offset);
			write<gphen0>(reg_value);
			break;
		case 1:
			/* Rising */
			reg_value = read<gpren1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren1>(reg_value);

			/* Low */
			reg_value = read<gplen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen1>(reg_value);

			/* Async Rising */
			reg_value = read<gparen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen1>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen1>(reg_value);

			/* Falling */
			reg_value = read<gpfen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen1>(reg_value);

			/* High */
			reg_value = read<gphen1>();
			reg_value |= (0x1 << gpio_offset);
			write<gphen1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void set_gpio_low_detect(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			/* Rising */
			reg_value = read<gpren0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren0>(reg_value);

			/* High */
			reg_value = read<gphen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen0>(reg_value);

			/* Async Rising */
			reg_value = read<gparen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen0>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen0>(reg_value);

			/* Falling */
			reg_value = read<gpfen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen0>(reg_value);

			/* Low */
			reg_value = read<gplen0>();
			reg_value |= (0x1 << gpio_offset);
			write<gplen0>(reg_value);
			break;
		case 1:
			/* Rising */
			reg_value = read<gpren1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren1>(reg_value);

			/* High */
			reg_value = read<gphen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen1>(reg_value);

			/* Async Rising */
			reg_value = read<gparen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen1>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen1>(reg_value);

			/* Falling */
			reg_value = read<gpfen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen1>(reg_value);

			/* Low */
			reg_value = read<gplen1>();
			reg_value |= (0x1 << gpio_offset);
			write<gplen1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void set_gpio_async_falling_detect(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			/* Rising */
			reg_value = read<gpren0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren0>(reg_value);

			/* High */
			reg_value = read<gphen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen0>(reg_value);

			/* Low */
			reg_value = read<gplen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen0>(reg_value);

			/* Async Rising */
			reg_value = read<gparen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen0>(reg_value);

			/* Falling */
			reg_value = read<gpfen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen0>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen0>();
			reg_value |= (0x1 << gpio_offset);
			write<gpafen0>(reg_value);
			break;
		case 1:
			/* Rising */
			reg_value = read<gpren1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren1>(reg_value);

			/* High */
			reg_value = read<gphen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen1>(reg_value);

			/* Low */
			reg_value = read<gplen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen1>(reg_value);

			/* Async Rising */
			reg_value = read<gparen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gparen1>(reg_value);

			/* Falling */
			reg_value = read<gpfen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen1>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen1>();
			reg_value |= (0x1 << gpio_offset);
			write<gpafen1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	void set_gpio_async_rising_detect(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			/* Rising */
			reg_value = read<gpren0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren0>(reg_value);

			/* High */
			reg_value = read<gphen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen0>(reg_value);

			/* Low */
			reg_value = read<gplen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen0>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen0>(reg_value);

			/* Falling */
			reg_value = read<gpfen0>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen0>(reg_value);

			/* Async Rising */
			reg_value = read<gparen0>();
			reg_value |= (0x1 << gpio_offset);
			write<gparen0>(reg_value);
			break;
		case 1:
			/* Rising */
			reg_value = read<gpren1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpren1>(reg_value);

			/* High */
			reg_value = read<gphen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gphen1>(reg_value);

			/* Low */
			reg_value = read<gplen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gplen1>(reg_value);

			/* Async Falling */
			reg_value = read<gpafen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpafen1>(reg_value);

			/* Falling */
			reg_value = read<gpfen1>();
			reg_value &= ~(0x1 << gpio_offset);
			write<gpfen1>(reg_value);

			/* Async Rising */
			reg_value = read<gparen1>();
			reg_value |= (0x1 << gpio_offset);
			write<gparen1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}

	unsigned int get_gpio_status0()
	{
		return read<gppeds0>();
	}

	unsigned int get_gpio_status1()
	{
		return read<gppeds1>();
	}

	void clear_event(unsigned gpio)
	{
		int gpio_offset = gpio%32;
		int idx = gpio/32;
		unsigned int reg_value;
		switch(idx){
		case 0:
			reg_value = read<gppeds0>();
			reg_value |= (0x1 << gpio_offset);
			write<gppeds0>(reg_value);
			break;
		case 1:
			reg_value = read<gppeds1>();
			reg_value |= (0x1 << gpio_offset);
			write<gppeds1>(reg_value);
			break;
		default:
			PERR("Wrong GPIO pin number: %d.",gpio);
		}
	}
};
#endif /* _GPIO_H_ */
