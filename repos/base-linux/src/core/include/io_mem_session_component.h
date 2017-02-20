/*
 * \brief  Core-specific instance of the IO_MEM session interface (Linux)
 * \author Christian Helmuth
 * \date   2007-09-14
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <io_mem_session/io_mem_session.h>

/* core includes */
#include <dataspace_component.h>

namespace Genode {

	class Io_mem_session_component : public Rpc_object<Io_mem_session>
	{
		public:

			/**
			 * Constructor
			 *
			 * \param io_mem_alloc  MMIO region allocator
			 * \param ram_alloc     RAM allocator that will be checked for
			 *                      region collisions
			 * \param ds_ep         entry point to manage the dataspace
			 *                      corresponding the io_mem session
			 * \param args          session construction arguments, in
			 *                      particular MMIO region base, size and
			 *                      caching demands
			 */
			Io_mem_session_component(Range_allocator *io_mem_alloc,
			                         Range_allocator *ram_alloc,
			                         Rpc_entrypoint  *ds_ep,
			                         const char      *args);

			/**
			 * Destructor
			 */
			~Io_mem_session_component() { }


			/*****************************
			 ** Io-mem session interface **
			 *****************************/

			Io_mem_dataspace_capability dataspace() {
				return Io_mem_dataspace_capability(); }
	};
}

#endif /* _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_ */
