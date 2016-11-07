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

/* core includes */
#include <io_port_session_component.h>

using namespace Genode;


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
