/*
 * \brief  Implementation of the IO_PORT session interface
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <kernel/log.h>

/* core includes */
#include <io_port_session_component.h>

using namespace Genode;


unsigned char Io_port_session_component::inb(unsigned short address)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
	return 0;
}


unsigned short Io_port_session_component::inw(unsigned short address)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
	return 0;
}


unsigned Io_port_session_component::inl(unsigned short address)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
	return 0;
}


void Io_port_session_component::outb(unsigned short address,
                                     unsigned char value)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}


void Io_port_session_component::outw(unsigned short address,
                                     unsigned short value)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}


void Io_port_session_component::outl(unsigned short address, unsigned value)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}


Io_port_session_component::
Io_port_session_component(Range_allocator * io_port_alloc,
                          const char * args)
: _io_port_alloc(io_port_alloc)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}


Io_port_session_component::~Io_port_session_component()
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}

