/*
 * \brief  Implementation of the IO_PORT session interface
 * \author Martin Stein
 * \date   2010-09-09
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "io_port_session_component.h"

using namespace Genode;


/**************
 ** Port API **
 **************/

unsigned char Io_port_session_component::inb(unsigned short)  { return 0; }
unsigned short Io_port_session_component::inw(unsigned short) { return 0; }
unsigned Io_port_session_component::inl(unsigned short)       { return 0; }

void Io_port_session_component::outb(unsigned short, unsigned char)  { }
void Io_port_session_component::outw(unsigned short, unsigned short) { }
void Io_port_session_component::outl(unsigned short, unsigned)       { }


/******************************
 ** Constructor / destructor **
 ******************************/

Io_port_session_component::Io_port_session_component(Range_allocator *io_port_alloc,
                                                     const char      *args)
: _io_port_alloc(io_port_alloc) { }


Io_port_session_component::~Io_port_session_component() { }
