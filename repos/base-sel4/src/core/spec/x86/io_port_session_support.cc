/*
 * \brief  Core implementation of the IO_PORT session interface
 * \author Alexander Boettcher
 * \date   2016-07-17
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <io_port_session_component.h>

#include <sel4/sel4.h>

using namespace Genode;


/**************
 ** Port API **
 **************/
unsigned char Io_port_session_component::inb(unsigned short address)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned char))) return 0;

	seL4_X86_IOPort_In8_t v = seL4_X86_IOPort_In8(seL4_CapIOPort, address);
	if (v.error == seL4_NoError)
		return v.result;

	Genode::error(__PRETTY_FUNCTION__, " failed ", v.error);
	return 0;
}


unsigned short Io_port_session_component::inw(unsigned short address)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned short))) return 0;

	seL4_X86_IOPort_In16_t v = seL4_X86_IOPort_In16(seL4_CapIOPort, address);
	if (v.error == seL4_NoError)
		return v.result;

	Genode::error(__PRETTY_FUNCTION__, " failed ", v.error);
	return 0;
}


unsigned Io_port_session_component::inl(unsigned short address)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned))) return 0;

	seL4_X86_IOPort_In32_t v = seL4_X86_IOPort_In32(seL4_CapIOPort, address);
	if (v.error == seL4_NoError)
		return v.result;

	Genode::error(__PRETTY_FUNCTION__, " failed ", v.error);
	return 0;
}


void Io_port_session_component::outb(unsigned short address, unsigned char value)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned char))) return;

	int error = seL4_X86_IOPort_Out8(seL4_CapIOPort, address, value);

	if (error != seL4_NoError)
		Genode::error(__PRETTY_FUNCTION__, " failed ", error);
}


void Io_port_session_component::outw(unsigned short address, unsigned short value)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned short))) return;

	int error = seL4_X86_IOPort_Out16(seL4_CapIOPort, address, value);

	if (error != seL4_NoError)
		Genode::error(__PRETTY_FUNCTION__, " failed ", error);
}


void Io_port_session_component::outl(unsigned short address, unsigned value)
{
	/* check boundaries */
	if (!_in_bounds(address, sizeof(unsigned))) return;

	int error = seL4_X86_IOPort_Out32(seL4_CapIOPort, address, value);

	if (error != seL4_NoError)
		Genode::error(__PRETTY_FUNCTION__, " failed ", error);
}
