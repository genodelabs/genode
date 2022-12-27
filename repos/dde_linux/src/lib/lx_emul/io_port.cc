/*
 * \brief  Lx_emul backend for I/O port access
 * \author Josef Soentgen
 * \date   2022-02-22
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/io_port.h>


template<typename T>
T _io_port_in(unsigned short addr)
{
	using namespace Lx_kit;
	using namespace Genode;

	unsigned ret = 0;
	bool valid_ret = false;
	env().devices.for_each([&] (Device & d) {
		if (d.io_port(addr)) {
			valid_ret = true;
			switch (sizeof (T)) {
			case sizeof (unsigned char):  ret = d.io_port_inb(addr); break;
			case sizeof (unsigned short): ret = d.io_port_inw(addr); break;
			case sizeof (unsigned int):   ret = d.io_port_inl(addr); break;
			default:                                   valid_ret = false; break;
			}
		}
	});

	if (!valid_ret)
		error("could not read I/O port resource ", Hex(addr));

	return static_cast<T>(ret);
}


unsigned char lx_emul_io_port_inb(unsigned short addr)
{
	return _io_port_in<unsigned char>(addr);
}


unsigned short lx_emul_io_port_inw(unsigned short addr)
{
	return _io_port_in<unsigned short>(addr);
}


unsigned int lx_emul_io_port_inl(unsigned short addr)
{
	return _io_port_in<unsigned int>(addr);
}

template<typename T>
void _io_port_out(unsigned short addr, T value)
{
	using namespace Lx_kit;
	using namespace Genode;

	bool valid_ret = false;
	env().devices.for_each([&] (Device & d) {
		if (d.io_port(addr)) {
			valid_ret = true;
			switch (sizeof (T)) {
			case sizeof (unsigned char):  d.io_port_outb(addr, (unsigned char)value); break;
			case sizeof (unsigned short): d.io_port_outw(addr, (unsigned short)value); break;
			case sizeof (unsigned int):   d.io_port_outl(addr, (unsigned int)value); break;
			default:                                     valid_ret = false; break;
			}
		}
	});

	if (!valid_ret)
		error("could not write I/O port resource ", Hex(addr));
}


void lx_emul_io_port_outb(unsigned char value, unsigned short addr)
{
	_io_port_out<unsigned char>(addr, value);
}


void lx_emul_io_port_outw(unsigned short value, unsigned short addr)
{
	_io_port_out<unsigned short>(addr, value);
}


void lx_emul_io_port_outl(unsigned int value, unsigned short addr)
{
	_io_port_out<unsigned int>(addr, value);
}
