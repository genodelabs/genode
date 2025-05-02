/*
 * \brief  Core-specific instance of the IO_PORT session interface
 * \author Christian Helmuth
 * \date   2007-04-17
 *
 * We assume core is running on IOPL3.
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IO_PORT_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IO_PORT_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <io_port_session/io_port_session.h>

/* core includes */
#include <types.h>

namespace Core { struct Io_port_session_component; }


struct Core::Io_port_session_component :  Rpc_object<Io_port_session>
{
	Range_allocator::Result const _io_port_range;

	bool _in_bounds(unsigned short addr, unsigned width)
	{
		return _io_port_range.convert<bool>(
			[&] (Range_allocator::Allocation const &a) {
				addr_t const base = addr_t(a.ptr);
				return (addr >= base) && (addr + width <= base + a.num_bytes); },
			[&] (Alloc_error) { return false; });
	}

	/**
	 * Constructor
	 *
	 * \param io_port_alloc  IO_PORT region allocator
	 * \param args           session construction arguments, in
	 *                       particular port base and size
	 */
	Io_port_session_component(Range_allocator &io_port_alloc, const char *args);


	/*******************************
	 ** Io-port session interface **
	 *******************************/

	unsigned char  inb(unsigned short) override;
	unsigned short inw(unsigned short) override;
	unsigned       inl(unsigned short) override;

	void outb(unsigned short, unsigned char)  override;
	void outw(unsigned short, unsigned short) override;
	void outl(unsigned short, unsigned)       override;
};

#endif /* _CORE__INCLUDE__IO_PORT_SESSION_COMPONENT_H_ */
