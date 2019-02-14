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

#include "io_port_session_component.h"

namespace Genode {

	struct Io_port_handler
	{
		private:

			enum { STACK_SIZE = 4096 };
			Rpc_entrypoint _ep;

		public:

			Io_port_handler(Pd_session &pd_session) :
				_ep(&pd_session, STACK_SIZE, "ioport")
			{ }

			Rpc_entrypoint &entrypoint() { return _ep; }
	};

	class Io_port_root : private Io_port_handler,
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
			 * \param cap_session    capability allocator
			 * \param io_port_alloc  platform IO_PORT allocator
			 * \param md_alloc       meta-data allocator to be used by root component
			 */
			Io_port_root(Pd_session        &pd_session,
			             Range_allocator   &io_port_alloc,
			             Allocator         &md_alloc)
			:
				Io_port_handler(pd_session),
				Root_component<Io_port_session_component>(&entrypoint(), &md_alloc),
				_io_port_alloc(io_port_alloc) { }
	};
}

#endif /* _CORE__INCLUDE__IO_PORT_ROOT_H_ */
