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

#ifndef _DRIVERS__GPIO__SPEC__RPI__GPIO_H_
#define _DRIVERS__GPIO__SPEC__RPI__GPIO_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <util/mmio.h>
#include <timer_session/connection.h>

namespace Gpio {

	using namespace Genode;

	class Reg;
}

class Gpio::Reg : Attached_io_mem_dataspace, Mmio<0xa0>
{
	private:

		/**
		 * GPIO Function Select Registers
		 */
		struct Gpfsel : Register_array <0x00,32,6,32> {
			struct Sel0 : Bitfield <0,3> {};
			struct Sel1 : Bitfield <3,3> {};
			struct Sel2 : Bitfield <6,3> {};
			struct Sel3 : Bitfield <9,3> {};
			struct Sel4 : Bitfield <12,3> {};
			struct Sel5 : Bitfield <15,3> {};
			struct Sel6 : Bitfield <18,3> {};
			struct Sel7 : Bitfield <21,3> {};
			struct Sel8 : Bitfield <24,3> {};
			struct Sel9 : Bitfield <27,3> {};
		};

		/**
		 * GPIO Pin Output Set Registers
		 */
		struct Gpset : Register_array <0x1c,32,64,1> {};

		/**
		 * GPIO Pin Output Clear Registers
		 */
		struct Gpclr : Register_array <0x28,32,64,1> {};

		/**
		 * GPIO Pin Level Registers
		 */
		struct Gplev : Register_array <0x34,32,64,1> {};

		/**
		 * GPIO Pin Event Detect Status Registers
		 */
		struct Gppeds     : Register_array <0x40,32,64,1> {};
		struct Gppeds_raw : Register       <0x40,64> {};

		/**
		 * GPIO Pin Rising Edge Detect Enable Registers
		 */
		struct Gpren : Register_array <0x4c,32,64,1> {};

		/**
		 * GPIO Pin Falling Edge Detect Enable Registers
		 */
		struct Gpfen : Register_array <0x58,32,64,1> {};

		/**
		 * GPIO Pin High Detect Enable Registers
		 */
		struct Gphen : Register_array <0x64,32,64,1> {};

		/**
		 * GPIO Pin Low Detect Enable Registers
		 */
		struct Gplen : Register_array <0x70,32,64,1> {};

		/**
		 * GPIO Pin Aync. Rising Edge Detect Registers
		 */
		struct Gparen : Register_array <0x7c,32,64,1> {};

		/**
		 * GPIO Pin Async. Falling Edge Detect Registers
		 */
		struct Gpafen : Register_array <0x88,32,64,1> {};

		/**
		 * GPIO Pin Pull-up/down Enable Registers
		 */
		struct Gppud : Register <0x94,32> {};

		/**
		 * GPIO Pin Pull-up/down Enable Clock Registers
		 */
		struct Gppudclk : Register_array <0x98,32,64,1> {};

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			Timer_delayer(Genode::Env &env) : Timer::Connection(env) { }

			/**
			 * Implementation of 'Delayer' interface
			 */
			void usleep(uint64_t us) override { Timer::Connection::usleep(us); }

		} _delayer;

		template <typename T0, typename T1, typename T2,
		          typename T3, typename T4, typename T5>
		void _set_gpio_det(unsigned gpio)
		{
			write<T0>(0, gpio);
			write<T1>(0, gpio);
			write<T2>(0, gpio);
			write<T3>(0, gpio);
			write<T4>(0, gpio);
			write<T5>(1, gpio);
		}

	public:

		Reg(Genode::Env &env,
		    addr_t base, off_t offset, size_t size)
		:
			Attached_io_mem_dataspace(env, base, size),
			Mmio({local_addr<char>() + offset, size - offset}),
			_delayer(env)
		{ }

		enum Function {
			FSEL_INPUT  = 0,
			FSEL_OUTPUT = 1,
			FSEL_ALT0   = 4,
			FSEL_ALT1   = 5,
			FSEL_ALT2   = 6,
			FSEL_ALT3   = 7,
			FSEL_ALT4   = 3,
			FSEL_ALT5   = 2,
		};

		void set_gpio_function(unsigned gpio, Function function)
		{
			/*
			 * Set a pull-up internal resistor in the input pin to avoid
			 * electromagnetic radiation or static noise
			 */
			if (function == FSEL_INPUT) {
				write<Gppud>(1);
				_delayer.usleep(1);
				write<Gppudclk>(1, gpio);
				_delayer.usleep(1);
				write<Gppud>(0);
				write<Gppudclk>(0, gpio);
			}
			/* set the pin function */
			unsigned sel_id = gpio % 10;
			unsigned reg_id = gpio / 10;
			switch(sel_id){
			case 0: write<Gpfsel::Sel0>(function, reg_id); break;
			case 1: write<Gpfsel::Sel1>(function, reg_id); break;
			case 2: write<Gpfsel::Sel2>(function, reg_id); break;
			case 3: write<Gpfsel::Sel3>(function, reg_id); break;
			case 4: write<Gpfsel::Sel4>(function, reg_id); break;
			case 5: write<Gpfsel::Sel5>(function, reg_id); break;
			case 6: write<Gpfsel::Sel6>(function, reg_id); break;
			case 7: write<Gpfsel::Sel7>(function, reg_id); break;
			case 8: write<Gpfsel::Sel8>(function, reg_id); break;
			case 9: write<Gpfsel::Sel9>(function, reg_id); break;
			default:;
			}
		}

		unsigned get_gpio_function(unsigned gpio)
		{
			unsigned sel_id = gpio % 10;
			unsigned reg_id = gpio / 10;
			switch(sel_id){
			case 0:  return read<Gpfsel::Sel0>(reg_id);
			case 1:  return read<Gpfsel::Sel1>(reg_id);
			case 2:  return read<Gpfsel::Sel2>(reg_id);
			case 3:  return read<Gpfsel::Sel3>(reg_id);
			case 4:  return read<Gpfsel::Sel4>(reg_id);
			case 5:  return read<Gpfsel::Sel5>(reg_id);
			case 6:  return read<Gpfsel::Sel6>(reg_id);
			case 7:  return read<Gpfsel::Sel7>(reg_id);
			case 8:  return read<Gpfsel::Sel8>(reg_id);
			case 9:  return read<Gpfsel::Sel9>(reg_id);
			default: return 0;
			}
		}

		int    get_gpio_level(unsigned gpio) { return read<Gppudclk>(gpio); }
		void   set_gpio_level(unsigned gpio) { write<Gpset>(1, gpio); }
		void clear_gpio_level(unsigned gpio) { write<Gpclr>(1, gpio); }

		void set_gpio_falling_detect(unsigned gpio) {
			_set_gpio_det<Gpren, Gphen, Gplen, Gparen, Gpafen, Gpfen>(gpio); }

		void set_gpio_rising_detect(unsigned gpio) {
			_set_gpio_det<Gphen, Gplen, Gparen, Gpafen, Gpfen, Gpren>(gpio); }

		void set_gpio_high_detect(unsigned gpio) {
			_set_gpio_det<Gpren, Gplen, Gparen, Gpafen, Gpfen, Gphen>(gpio); }

		void set_gpio_low_detect(unsigned gpio) {
			_set_gpio_det<Gpren, Gphen, Gparen, Gpafen, Gpfen, Gplen>(gpio); }

		void set_gpio_async_falling_detect(unsigned gpio) {
			_set_gpio_det<Gpren, Gphen, Gplen, Gparen, Gpfen, Gpafen>(gpio); }

		void set_gpio_async_rising_detect(unsigned gpio) {
			_set_gpio_det<Gpren, Gphen, Gplen, Gpafen, Gpfen, Gparen>(gpio); }

		template <typename F>
		void for_each_gpio_status(F f)
		{
			Gppeds_raw::access_t const gppeds = read<Gppeds_raw>();
			for(unsigned i = 0; i < Gppeds_raw::ACCESS_WIDTH; i++) {
				f(i, gppeds & (1 << i)); }
		}

		void clear_event(unsigned gpio) { write<Gppeds>(1, gpio); }
};

#endif /* _DRIVERS__GPIO__SPEC__RPI__GPIO_H_ */
