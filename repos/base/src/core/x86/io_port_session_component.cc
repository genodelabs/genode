/*
 * \brief  Core implementation of the IO_PORT session interface
 * \author Christian Helmuth
 * \date   2007-04-17
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>
#include <util/arg_string.h>
#include <root/root.h>

/* core includes */
#include <io_port_session_component.h>

using namespace Genode;


static const bool verbose = false;


/**************
 ** Port API **
 **************/

unsigned char Io_port_session_component::inb(unsigned short address)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned char))) return 0;

	unsigned char v;
	asm volatile ("inb %w1, %b0" : "=a" (v) : "Nd" (address));
	return v;
}


unsigned short Io_port_session_component::inw(unsigned short address)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned short))) return 0;

	unsigned short v;
	asm volatile ("inw %w1, %w0" : "=a" (v) : "Nd" (address));
	return v;
}


unsigned Io_port_session_component::inl(unsigned short address)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned))) return 0;

	unsigned v;
	asm volatile ("inl %w1, %0" : "=a" (v) : "Nd" (address));
	return v;
}


void Io_port_session_component::outb(unsigned short address, unsigned char value)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned char))) return;

	asm volatile ("outb %b0, %w1" : : "a" (value), "Nd" (address));
}


void Io_port_session_component::outw(unsigned short address, unsigned short value)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned short))) return;

	asm volatile ("outw %w0, %w1" : : "a" (value), "Nd" (address));
}


void Io_port_session_component::outl(unsigned short address, unsigned value)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned))) return;

	asm volatile ("outl %0, %w1" : : "a" (value), "Nd" (address));
}


/******************************
 ** Constructor / destructor **
 ******************************/

Io_port_session_component::Io_port_session_component(Range_allocator *io_port_alloc,
                                                     const char      *args)
: _io_port_alloc(io_port_alloc)
{
	/* parse for port properties */
	unsigned base = Arg_string::find_arg(args, "io_port_base").ulong_value(0);
	unsigned size = Arg_string::find_arg(args, "io_port_size").ulong_value(0);

	/* allocate region (also checks out-of-bounds regions) */
	switch (io_port_alloc->alloc_addr(size, base).value) {

	case Range_allocator::Alloc_return::RANGE_CONFLICT:
		PERR("I/O port [%x,%x) not available", base, base + size);
		throw Root::Invalid_args();

	case Range_allocator::Alloc_return::OUT_OF_METADATA:
		PERR("I/O port allocator ran out of meta data");

		/*
		 * Do not throw 'Quota_exceeded' because the client cannot do
		 * anything about the meta data allocator of I/O ports.
		 */
		throw Root::Invalid_args();

	case Range_allocator::Alloc_return::OK: break;
	}

	if (verbose)
		PDBG("I/O port: [%04x,%04x)", base, base + size);

	/* store information */
	_base = base;
	_size = size;
}


Io_port_session_component::~Io_port_session_component()
{
	if (verbose)
		PDBG("I/O port: [%04x,%04x)", _base, _base + _size);

	_io_port_alloc->free(reinterpret_cast<void *>(_base));
}
