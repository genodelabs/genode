/*
 * \brief  Linux-specific IO_PORT service
 * \author Christian Helmuth
 * \date   2007-04-18
 */

/*
 * Copyright (C) 2007-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <root/root.h>

#include "io_port_session_component.h"

using namespace Genode;


unsigned char Io_port_session_component::inb(unsigned short address) {
	return 0; }


unsigned short Io_port_session_component::inw(unsigned short address) {
	return 0; }


unsigned Io_port_session_component::inl(unsigned short address) {
	return 0; }


void Io_port_session_component::outb(unsigned short address, unsigned char value) {
}


void Io_port_session_component::outw(unsigned short address, unsigned short value) {
}


void Io_port_session_component::outl(unsigned short address, unsigned value) {
}


Io_port_session_component::Io_port_session_component(Range_allocator *io_port_alloc,
                                                     const char      *args)
: _io_port_alloc(io_port_alloc)
{
	PWRN("no IO_PORT support under Linux (args=\"%s\")", args);
	_size = 0;
	throw Root::Invalid_args();
}


Io_port_session_component::~Io_port_session_component()
{
	PERR("Implement me, immediately!");
}
