/*
 * \brief  IO_PORT root interface
 * \author Christian Helmuth
 * \date   2007-04-17
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IO_PORT_ROOT_H_
#define _CORE__INCLUDE__IO_PORT_ROOT_H_

#include <base/thread.h>
#include <root/component.h>

/* core includes */
#include <io_port_session_component.h>
#include <types.h>

namespace Core {
	class  Io_port_handler;
	struct Io_port_root;
}


struct Core::Io_port_handler : Rpc_entrypoint
{
	Io_port_handler(Runtime &runtime)
	:
		Rpc_entrypoint(runtime, "ioport", Genode::Thread::Stack_size { 4096 },
		               Genode::Thread::Location { })
	{ }
};


struct Core::Io_port_root : private Io_port_handler,
                            Root_component<Io_port_session_component>
{
	Range_allocator &_io_port_alloc;  /* I/O port allocator */

	Create_result _create_session(const char *args) override
	{
		return _alloc_obj(_io_port_alloc, args);
	}

	/**
	 * Constructor
	 *
	 * \param io_port_alloc  platform IO_PORT allocator
	 * \param md_alloc       meta-data allocator to be used by root component
	 */
	Io_port_root(Runtime &runtime, Range_allocator &io_port_alloc, Allocator &md_alloc)
	:
		Io_port_handler(runtime),
		Root_component<Io_port_session_component>(this, &md_alloc),
		_io_port_alloc(io_port_alloc)
	{ }
};

#endif /* _CORE__INCLUDE__IO_PORT_ROOT_H_ */
