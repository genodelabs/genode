/*
 * \brief  IO_MEM root interface
 * \author Christian Helmuth
 * \date   2006-08-01
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IO_MEM_ROOT_H_
#define _CORE__INCLUDE__IO_MEM_ROOT_H_

#include <root/component.h>

#include "io_mem_session_component.h"

namespace Genode {

	class Io_mem_root : public Root_component<Io_mem_session_component>
	{

		private:

			Range_allocator *_io_mem_alloc;  /* MMIO region allocator */
			Range_allocator *_ram_alloc;     /* RAM allocator */
			Rpc_entrypoint  *_ds_ep;         /* entry point for managing io_mem dataspaces */

		protected:

			Io_mem_session_component *_create_session(const char *args)
			{
				return new (md_alloc())
					Io_mem_session_component(_io_mem_alloc, _ram_alloc,
				                             _ds_ep, args);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep    entry point for managing io_mem session objects
			 * \param ds_ep         entry point for managing dataspaces
			 * \param io_mem_alloc  platform IO_MEM allocator
			 * \param ram_alloc     platform RAM allocator
			 * \param md_alloc      meta-data allocator to be used by root component
			 */
			Io_mem_root(Rpc_entrypoint  *session_ep,
			            Rpc_entrypoint  *ds_ep,
			            Range_allocator *io_mem_alloc,
			            Range_allocator *ram_alloc,
			            Allocator       *md_alloc)
			:
				Root_component<Io_mem_session_component>(session_ep, md_alloc),
				_io_mem_alloc(io_mem_alloc), _ram_alloc(ram_alloc), _ds_ep(ds_ep) { }
	};
}

#endif /* _CORE__INCLUDE__IO_MEM_ROOT_H_ */
