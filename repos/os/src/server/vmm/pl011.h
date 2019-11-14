/*
 * \brief  VMM PL011 serial device model
 * \author Stefan Kalkowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__PL011_H_
#define _SRC__SERVER__VMM__PL011_H_

#include <os/ring_buffer.h>
#include <terminal_session/connection.h>

#include <gic.h>

namespace Vmm {
	class Cpu;
	class Pl011;
}

class Vmm::Pl011 : public Vmm::Mmio_device
{
	private:

		using Ring_buffer =
			Genode::Ring_buffer<char, 1024,
			                    Genode::Ring_buffer_unsynchronized>;

		enum Mask : Genode::uint16_t {
			RX_MASK = 1 << 4,
			TX_MASK = 1 << 5
		};

		Terminal::Connection       _terminal;
		Cpu::Signal_handler<Pl011> _handler;
		Gic::Irq                 & _irq;
		Ring_buffer                _rx_buf;
		Mmio_register              _uart_ris { "UARTRIS", Mmio_register::RO,
		                                       0x3c, 2 };

		/**
		 * Dummy container for holding array of noncopyable objects
		 * Workaround for gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70395
		 */
		struct Dummy {
			Mmio_register regs[13];
		} _reg_container { .regs = {
			{ "UARTIBRD",      Mmio_register::RW, 0x24,  2, 0     },
			{ "UARTFBRD",      Mmio_register::RW, 0x28,  2, 0     },
			{ "UARTLCR_H",     Mmio_register::RW, 0x2c,  2, 0     },
			{ "UARTCR",        Mmio_register::RW, 0x30,  2, 0x300 },
			{ "UARTIFLS",      Mmio_register::RW, 0x34,  2, 0x12  },
			{ "UARTPERIPHID0", Mmio_register::RO, 0xfe0, 4, 0x11  },
			{ "UARTPERIPHID1", Mmio_register::RO, 0xfe4, 4, 0x10  },
			{ "UARTPERIPHID2", Mmio_register::RO, 0xfe8, 4, 0x14  },
			{ "UARTPERIPHID3", Mmio_register::RO, 0xfec, 4, 0x0   },
			{ "UARTPCELLID0",  Mmio_register::RO, 0xff0, 4, 0xd   },
			{ "UARTPCELLID1",  Mmio_register::RO, 0xff4, 4, 0xf0  },
			{ "UARTPCELLID2",  Mmio_register::RO, 0xff8, 4, 0x5   },
			{ "UARTPCELLID3",  Mmio_register::RO, 0xffc, 4, 0xb1  }
		}};

		struct Uartdr : Mmio_register
		{
			Terminal::Connection & terminal;
			Ring_buffer          & rx;
			Mmio_register        & ris;

			Register read(Address_range&,  Cpu&)           override;
			void     write(Address_range&, Cpu&, Register) override;

			Uartdr(Terminal::Connection & terminal,
			       Ring_buffer          & rx,
			       Mmio_register        & ris)
			: Mmio_register("UARTDR", Mmio_register::RW, 0x0, 2),
			  terminal(terminal), rx(rx), ris(ris) {}
		} _uart_dr { _terminal, _rx_buf, _uart_ris };

		struct Uartfr : Mmio_register, Genode::Register<32>
		{
			struct Rx_empty : Bitfield<4, 1> {};
			struct Rx_full  : Bitfield<6, 1> {};

			Ring_buffer & rx;

			Mmio_register::Register read(Address_range&, Cpu&) override;

			Uartfr(Ring_buffer & rx)
			: Mmio_register("UARTFR", Mmio_register::RO, 0x18, 4), rx(rx) {}
		} _uart_fr { _rx_buf };

		struct Uartimsc : Mmio_register
		{
			Gic::Irq      & irq;
			Mmio_register & ris;

			void write(Address_range&, Cpu&, Register) override;

			Uartimsc(Gic::Irq & irq, Mmio_register & ris)
			: Mmio_register("UARTIMSC", Mmio_register::RW, 0x38, 2, 0xf),
			  irq(irq), ris(ris) {}
		} _uart_imsc { _irq, _uart_ris };

		struct Uartmis : Mmio_register
		{
			Mmio_register & ris;
			Uartimsc      & imsc;

			Register read(Address_range&,  Cpu&) override;

			Uartmis(Mmio_register & ris, Uartimsc & imsc)
			: Mmio_register("UARTMIS", Mmio_register::RO, 0x40, 2),
			  ris(ris), imsc(imsc) {}
		} _uart_mis { _uart_ris, _uart_imsc };

		struct Uarticr : Mmio_register
		{
			Mmio_register & ris;

			void write(Address_range&, Cpu&, Register) override;

			Uarticr(Mmio_register & ris)
			: Mmio_register("UARTICR", Mmio_register::WO, 0x44, 2), ris(ris) {}
		} _uart_icr { _uart_ris };

		void _read();

	public:

		Pl011(const char * const       name,
		      const Genode::uint64_t   addr,
		      const Genode::uint64_t   size,
		      unsigned                 irq,
		      Cpu                    & cpu,
		      Mmio_bus               & bus,
		      Genode::Env            & env);
};

#endif /* _SRC__SERVER__VMM__PL011_H_ */
