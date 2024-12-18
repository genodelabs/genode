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

#include <root/component.h>

/* core includes */
#include <io_port_session_component.h>

namespace Core {
	class Io_port_handler;
	class Io_port_root;
}


struct Core::Io_port_handler
{
	static constexpr size_t STACK_SIZE = 4096;

	Rpc_entrypoint handler_ep { nullptr, STACK_SIZE, "ioport", Affinity::Location() };
};


class Core::Io_port_root : private Io_port_handler,
                           public Root_component<Io_port_session_component>
{
	private:

		Range_allocator &_io_port_alloc;  /* I/O port allocator */

	protected:

		Io_port_session_component *_create_session(const char *args) override {
			return new (md_alloc()) Io_port_session_component(_io_port_alloc, args); }

	public:

		/**
		 * Constructor
		 *
		 * \param io_port_alloc  platform IO_PORT allocator
		 * \param md_alloc       meta-data allocator to be used by root component
		 */
		Io_port_root(Range_allocator &io_port_alloc, Allocator &md_alloc)
		:
			Root_component<Io_port_session_component>(&handler_ep, &md_alloc),
			_io_port_alloc(io_port_alloc)
		{ }
};

#endif /* _CORE__INCLUDE__IO_PORT_ROOT_H_ */
