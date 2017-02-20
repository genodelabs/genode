/*
 * \brief  I/O-port session interface
 * \author Christian Helmuth
 * \date   2007-04-17
 *
 * An I/O port session permits access to a range of ports. Inside this range
 * variable-sized accesses (i.e., 8, 16, 32 bit) at arbitrary addresses are
 * allowed - currently, alignment is not enforced. Core enforces that access is
 * limited to the session-defined range while the user provides physical I/O port
 * addresses as arguments.
 *
 * The design is founded on experiences while programming PCI configuration
 * space which needs two 32-bit port registers. Each byte, word and dword in
 * the data register must be explicitly accessible for read and write. The old
 * design needs six capabilities only for the data register.
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IO_PORT_SESSION__IO_PORT_SESSION_H_
#define _INCLUDE__IO_PORT_SESSION__IO_PORT_SESSION_H_

#include <base/capability.h>
#include <session/session.h>

namespace Genode { struct Io_port_session; }


struct Genode::Io_port_session : Session
{
	static const char *service_name() { return "IO_PORT"; }

	virtual ~Io_port_session() { }

	/******************************
	 ** Read value from I/O port **
	 ******************************/

	/**
	 * Read byte (8 bit)
	 *
	 * \param address  physical I/O port address
	 *
	 * \return         value read from port
	 */
	virtual unsigned char inb(unsigned short address) = 0;

	/**
	 * Read word (16 bit)
	 *
	 * \param address  physical I/O port address
	 *
	 * \return         value read from port
	 */
	virtual unsigned short inw(unsigned short address) = 0;

	/**
	 * Read double word (32 bit)
	 *
	 * \param address  physical I/O port address
	 *
	 * \return         value read from port
	 */
	virtual unsigned inl(unsigned short address) = 0;


	/*****************************
	 ** Write value to I/O port **
	 *****************************/

	/**
	 * Write byte (8 bit)
	 *
	 * \param address  physical I/O port address
	 * \param value    value to write to port
	 */
	virtual void outb(unsigned short address, unsigned char value) = 0;

	/**
	 * Write word (16 bit)
	 *
	 * \param address  physical I/O port address
	 * \param value    value to write to port
	 */
	virtual void outw(unsigned short address, unsigned short value) = 0;

	/**
	 * Write double word (32 bit)
	 *
	 * \param address  physical I/O port address
	 * \param value    value to write to port
	 */
	virtual void outl(unsigned short address, unsigned value) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_inb, unsigned char,  inb, unsigned short);
	GENODE_RPC(Rpc_inw, unsigned short, inw, unsigned short);
	GENODE_RPC(Rpc_inl, unsigned,       inl, unsigned short);

	GENODE_RPC(Rpc_outb, void, outb, unsigned short, unsigned char);
	GENODE_RPC(Rpc_outw, void, outw, unsigned short, unsigned short);
	GENODE_RPC(Rpc_outl, void, outl, unsigned short, unsigned);

	GENODE_RPC_INTERFACE(Rpc_inb, Rpc_inw, Rpc_inl, Rpc_outb, Rpc_outw, Rpc_outl);
};

#endif /* _INCLUDE__IO_PORT_SESSION__IO_PORT_SESSION_H_ */
