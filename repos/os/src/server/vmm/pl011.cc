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

#include <cpu.h>
#include <pl011.h>

using Vmm::Pl011;
using Register = Vmm::Mmio_register::Register;

Register Pl011::Uartdr::read(Address_range&,  Cpu&)
{
	ris.set(ris.value() & ~RX_MASK);

	if (!rx.empty()) return rx.get();

	return 0;
}


void Pl011::Uartdr::write(Address_range&, Cpu&, Register reg)
{
	terminal.write(&reg, 1);
}


Register Pl011::Uartfr::read(Address_range&,  Cpu&)
{
	return rx.empty() ? Rx_empty::bits(1) : Rx_full::bits(1);
}


void Pl011::Uartimsc::write(Address_range&, Cpu&, Register mask)
{
	if ((mask & RX_MASK) && !(value() & RX_MASK) && (ris.value() & RX_MASK))
		irq.assert();

	set(mask);
}


Register Pl011::Uartmis::read(Address_range&,  Cpu&)
{
	return ris.value() & imsc.value();
}


void Pl011::Uarticr::write(Address_range & ar, Cpu &, Register value)
{
	ris.set(ris.value() & ~value);
}


void Pl011::_read()
{
	if (!_terminal.avail()) return;

	while (_terminal.avail() && _rx_buf.avail_capacity()) {
		unsigned char c = 0;
		_terminal.read(&c, 1);
		_rx_buf.add(c);
	}

	_uart_ris.set(_uart_ris.value() | RX_MASK);
	if (_uart_imsc.value() & RX_MASK) _irq.assert();
}


Pl011::Pl011(const char * const       name,
             const Genode::uint64_t   addr,
             const Genode::uint64_t   size,
             unsigned                 irq,
             Cpu                    & cpu,
             Mmio_bus               & bus,
             Genode::Env            & env)
: Mmio_device(name, addr, size),
  _terminal(env, "earlycon"),
  _handler(cpu, env.ep(), *this, &Pl011::_read),
  _irq(cpu.gic().irq(irq))
{
	for (unsigned i = 0; i < (sizeof(Dummy::regs) / sizeof(Mmio_register)); i++)
		add(_reg_container.regs[i]);
	add(_uart_ris);
	add(_uart_dr);
	add(_uart_fr);
	add(_uart_imsc);
	add(_uart_mis);
	add(_uart_icr);

	_terminal.read_avail_sigh(_handler);

	bus.add(*this);
}
