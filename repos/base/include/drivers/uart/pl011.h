/*
 * \brief  Driver for the PrimeCell UART PL011 Revision r1p3
 * \author Martin stein
 * \date   2011-10-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__UART__PL011_H_
#define _INCLUDE__DRIVERS__UART__PL011_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode { class Pl011_uart; }


/**
 * Driver base for the PrimeCell UART PL011 Revision r1p3
 */
class Genode::Pl011_uart : Mmio<0x3a>
{
	protected:

		enum { MAX_BAUD_RATE = 0xfffffff };

		/**
		 * Data register
		 */
		struct Uartdr : public Register<0x00, 16>
		{
			struct Data : Bitfield<0,8>  { };
			struct Fe   : Bitfield<8,1>  { };
			struct Pe   : Bitfield<9,1>  { };
			struct Be   : Bitfield<10,1> { };
			struct Oe   : Bitfield<11,1> { };
		};

		/**
		 * Flag register
		 */
		struct Uartfr : public Register<0x18, 16>
		{
			struct Cts  : Bitfield<0,1> { };
			struct Dsr  : Bitfield<1,1> { };
			struct Dcd  : Bitfield<2,1> { };
			struct Busy : Bitfield<3,1> { };
			struct Rxfe : Bitfield<4,1> { };
			struct Txff : Bitfield<5,1> { };
			struct Rxff : Bitfield<6,1> { };
			struct Txfe : Bitfield<7,1> { };
			struct Ri   : Bitfield<8,1> { };
		};

		/**
		 * Integer baud rate register
		 */
		struct Uartibrd : public Register<0x24, 16>
		{
			struct Ibrd : Bitfield<0,15> { };
		};

		/**
		 * Fractional Baud Rate Register
		 */
		struct Uartfbrd : public Register<0x28, 8>
		{
			struct Fbrd : Bitfield<0,6> { };
		};

		/**
		 * Line Control Register
		 */
		struct Uartlcrh : public Register<0x2c, 16>
		{
			struct Wlen : Bitfield<5,2> {
				enum {
					WORD_LENGTH_8BITS = 3,
					WORD_LENGTH_7BITS = 2,
					WORD_LENGTH_6BITS = 1,
					WORD_LENGTH_5BITS = 0,
				};
			};
		};

		/**
		 * Control Register
		 */
		struct Uartcr : public Register<0x30, 16>
		{
			struct Uarten : Bitfield<0,1> { };
			struct Txe    : Bitfield<8,1> { };
			struct Rxe    : Bitfield<9,1> { };
		};

		/**
		 * Interrupt Mask Set/Clear
		 */
		struct Uartimsc : public Register<0x38, 16>
		{
			struct Imsc : Bitfield<0,11> { };
		};

		/**
		 * Idle until the device is ready for action
		 */
		void _wait_until_ready() { while (read<Uartfr::Busy>()) ; }

	public:

		/**
		 * Constructor
		 * \param  base       device MMIO base
		 * \param  clock      device reference clock frequency
		 * \param  baud_rate  targeted UART baud rate
		 */
		inline Pl011_uart(addr_t const base, uint32_t const clock,
		                  uint32_t const baud_rate);

		/**
		 * Send ASCII char 'c' over the UART interface
		 */
		inline void put_char(char const c);

		void init() { }
};


Genode::Pl011_uart::Pl011_uart(addr_t const base, uint32_t const clock,
                               uint32_t const baud_rate) : Mmio({(char *)base, Mmio::SIZE})
{
	write<Uartcr>(Uartcr::Uarten::bits(1) |
	              Uartcr::Txe::bits(1)    |
	              Uartcr::Rxe::bits(1));

	/*
	 * We can't print an error or throw C++ exceptions because we must expect
	 * both to be uninitialized yet, so its better to hold the program counter
	 * in here for debugging.
	 */
	if (baud_rate > MAX_BAUD_RATE) while(1) ;

	/*
	 * Calculate fractional and integer part of baud rate divisor to initialize
	 * IBRD and FBRD.
	 */
	uint32_t           const adjusted_br = baud_rate << 4;
	double             const divisor = (double)clock / adjusted_br;
	Uartibrd::access_t const ibrd = (Uartibrd::access_t)divisor;
	Uartfbrd::access_t const fbrd = (Uartfbrd::access_t)(((divisor - ibrd)
	                                * 64) + 0.5);

	write<Uartfbrd::Fbrd>(fbrd);
	write<Uartibrd::Ibrd>(ibrd);

	write<Uartlcrh::Wlen>(Uartlcrh::Wlen::WORD_LENGTH_8BITS);

	/* unmask all interrupts */
	write<Uartimsc::Imsc>(0);

	_wait_until_ready();
}


void Genode::Pl011_uart::put_char(char const c)
{
	/* wait as long as the transmission buffer is full */
	while (read<Uartfr::Txff>()) ;

	/* transmit character */
	write<Uartdr::Data>(c);
	_wait_until_ready();
}


#endif /* _INCLUDE__DRIVERS__UART__PL011_H_ */
