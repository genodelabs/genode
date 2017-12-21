/*
 * \brief  Pbxa9 UART driver
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2011-05-27
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/env.h>
#include <drivers/defs/pbxa9.h>

enum { UARTS_NUM = 4 }; /* needed by base class definitions */

/* local includes */
#include <uart_driver_base.h>

class Uart::Driver : private Genode::Attached_io_mem_dataspace,
                     public  Uart::Driver_base
{
	private:

		/*
		 * Noncopyable
		 */
		Driver(Driver const &);
		Driver &operator = (Driver const &);

		enum {

			/**
			 * MMIO regions
			 */
			PL011_PHYS0 = 0x10009000,  /* base for UART 0 */
			PL011_PHYS1 = 0x1000a000,  /* base for UART 1 */
			PL011_PHYS2 = 0x1000b000,  /* base for UART 2 */
			PL011_PHYS3 = 0x1000c000,  /* base for UART 3 */
			PL011_SIZE  = 0x1000,      /* size of each MMIO region */

			/**
			 * Interrupt lines
			 */
			PL011_IRQ0 = Pbxa9::PL011_0_IRQ, /* UART 0 */
			PL011_IRQ1 = Pbxa9::PL011_1_IRQ, /* UART 1 */
			PL011_IRQ2 = Pbxa9::PL011_2_IRQ, /* UART 2 */
			PL011_IRQ3 = Pbxa9::PL011_3_IRQ, /* UART 3 */

			/**
			 * UART baud rate configuration (precalculated)
			 *
			 * div  = 24000000 / 16 / baud rate
			 * IBRD = floor(div)
			 * FBRD = floor((div - IBRD) * 64 + 0.5)
			 */
			PL011_IBRD_115200 =  13, PL011_FBRD_115200 =  1,
			PL011_IBRD_19200  =  78, PL011_FBRD_19200  =  8,
			PL011_IBRD_9600   = 156, PL011_FBRD_9600   = 16,

			BAUD_115200 = 115200
		};

		struct Uart {
			Genode::addr_t mmio_base;
			Genode::size_t mmio_size;
			int            irq_number;
		};

		Uart & _config(unsigned index)
		{
			using namespace Genode;

			static Uart cfg[UARTS_NUM] = {
				{ PL011_PHYS0, PL011_SIZE, PL011_IRQ0 },
				{ PL011_PHYS1, PL011_SIZE, PL011_IRQ1 },
				{ PL011_PHYS2, PL011_SIZE, PL011_IRQ2 },
				{ PL011_PHYS3, PL011_SIZE, PL011_IRQ3 },
			};
			return cfg[index];
		}


		/*
		 * Constants for flags, registers, etc. only visible in this file are
		 * named as in the specification documents to relieve correlation
		 * between implementation and specification.
		 */

		/**
		 * Register offsets
		 *
		 * Registers are read/writable unless explicitly stated.
		 */
		enum Register {
			UARTDR    = 0x000,  /* data */
			UARTFR    = 0x018,  /* flags (read-only) */
			UARTIBRD  = 0x024,  /* integer baud rate divisor */
			UARTFBRD  = 0x028,  /* fractional baud rate divisor */
			UARTLCR_H = 0x02c,  /* line control */
			UARTCR    = 0x030,  /* control */
			UARTIMSC  = 0x038,  /* interrupt mask register */
			UARTICR   = 0x044,  /* interrupt clear register */
		};

		/**
		 * Flags
		 */
		enum Flag {
			/* flag register */
			UARTFR_BUSY      = 0x0008,  /* busy on tx */
			UARTFR_TXFF      = 0x0020,  /* tx FIFO full */
			UARTFR_RXFE      = 0x0010,  /* rx FIFO empty */

			/* line control register */
			UARTLCR_H_FEN    = 0x0010,  /* enable FIFOs */
			UARTLCR_H_WLEN_8 = 0x0060,  /* 8 bit word length */

			/* control register */
			UARTCR_UARTEN    = 0x0001,  /* enable uart */
			UARTCR_TXE       = 0x0100,  /* enable tx */
			UARTCR_RXE       = 0x0200,  /* enable rx */

			/* interrupt mask register */
			UARTIMSC_RXIM    = 0x10,    /* rx interrupt mask */

			/* interrupt clear register */
			UARTICR_RXIC     = 0x10,    /* rx interrupt clear */
		};

		Genode::uint32_t volatile *_base;

		Genode::uint32_t _read_reg(Register reg) const {
			return _base[reg >> 2]; }

		void _write_reg(Register reg, Genode::uint32_t v) {
			_base[reg >> 2] = v; }

	public:

		Driver(Genode::Env &env, unsigned index,
		       unsigned baud_rate, Char_avail_functor &func)
		: Genode::Attached_io_mem_dataspace(env, _config(index).mmio_base,
		                                     _config(index).mmio_size),
		  Driver_base(env, _config(index).irq_number, func),
		  _base(local_addr<unsigned volatile>())
		{
			if (baud_rate != BAUD_115200)
				Genode::warning("baud_rate ", baud_rate,
				                " not supported, set to default");

			/* disable and flush uart */
			_write_reg(UARTCR, 0);
			while (_read_reg(UARTFR) & UARTFR_BUSY) ;
			_write_reg(UARTLCR_H, 0);

			/* set baud-rate divisor */
			_write_reg(UARTIBRD, PL011_IBRD_115200);
			_write_reg(UARTFBRD, PL011_FBRD_115200);

			/* enable FIFOs, 8-bit words */
			_write_reg(UARTLCR_H, UARTLCR_H_FEN | UARTLCR_H_WLEN_8);

			/* enable transmission */
			_write_reg(UARTCR, UARTCR_TXE);

			/* enable uart */
			_write_reg(UARTCR, _read_reg(UARTCR) | UARTCR_UARTEN);

			/* enable rx interrupt */
			_write_reg(UARTIMSC, UARTIMSC_RXIM);
		}


		/***************************
		 ** UART driver interface **
		 ***************************/

		void handle_irq() override
		{
			/* inform client about the availability of data */
			Driver_base::handle_irq();

			/* acknowledge irq */
			_write_reg(UARTICR, UARTICR_RXIC);
		}

		void put_char(char c) override
		{
			/* spin while FIFO full */
			while (_read_reg(UARTFR) & UARTFR_TXFF) ;

			_write_reg(UARTDR, c);
		}

		bool char_avail() override {
			return !(_read_reg(UARTFR) & UARTFR_RXFE); }

		char get_char() override {
			return _read_reg(UARTDR); }
};

#endif /* _UART_DRIVER_H_ */
