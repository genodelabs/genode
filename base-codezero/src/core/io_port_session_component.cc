/*
 * \brief  Implementation of the IO_PORT session interface
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "io_port_session_component.h"

using namespace Genode;


/**************
 ** Port API **
 **************/

unsigned char Io_port_session_component::inb(unsigned short address) {
	return 0; }


unsigned short Io_port_session_component::inw(unsigned short address) {
	return 0; }


unsigned Io_port_session_component::inl(unsigned short address) {
	return 0; }


void Io_port_session_component::outb(unsigned short address, unsigned char value)
{ }


void Io_port_session_component::outw(unsigned short address, unsigned short value)
{ }


void Io_port_session_component::outl(unsigned short address, unsigned value)
{ }


/******************************
 ** Constructor / destructor **
 ******************************/

Io_port_session_component::Io_port_session_component(Range_allocator *io_port_alloc,
                                                     const char      *args)
: _io_port_alloc(io_port_alloc)
{ }


Io_port_session_component::~Io_port_session_component()
{ }
