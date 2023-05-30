/*
 * \brief  Core-specific instance of the IO_MEM session interface
 * \author Christian Helmuth
 * \date   2006-09-14
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
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

namespace Core { class Io_mem_session_component; }


class Core::Io_mem_session_component : public Rpc_object<Io_mem_session>
{
	private:

		/*
		 * Helper class used to pass the dataspace attributes as
		 * parameters from the _prepare_io_mem function to the
		 * constructor of Dataspace_component.
		 */
		struct Dataspace_attr
		{
			size_t size            { 0 };
			addr_t core_local_addr { 0 };
			addr_t phys_addr       { 0 };
			Cache  cacheable       { UNCACHED };

			/**
			 * Base address of request used for freeing mem-ranges
			 */
			addr_t req_base { 0 };

			/**
			 * Default constructor
			 *
			 * This constructor enables Dataspace_attr objects to be
			 * returned from the '_prepare_io_mem' function.
			 */
			Dataspace_attr() { }

			/**
			 * Constructor
			 *
			 * An invalid dataspace is represented by setting all
			 * arguments to zero.
			 */
			Dataspace_attr(size_t s, addr_t cla, addr_t pa, Cache c,
			               addr_t req_base)
			:
				size(s), core_local_addr(cla), phys_addr(pa),
				cacheable(c), req_base(req_base) { }
		};

		struct Io_dataspace_component : Dataspace_component
		{
			addr_t req_base;

			/**
			 * Constructor
			 */
			Io_dataspace_component(Dataspace_attr da)
			:
				Dataspace_component(da.size, da.core_local_addr,
				                    da.phys_addr, da.cacheable,
				                    true, 0),
				req_base(da.req_base) { }


			bool valid() { return size() != 0; }
		};

		Range_allocator            &_io_mem_alloc;
		Io_dataspace_component      _ds;
		Rpc_entrypoint             &_ds_ep;
		Io_mem_dataspace_capability _ds_cap    { };
		Cache                       _cacheable { UNCACHED };

		Dataspace_attr _prepare_io_mem(const char *args, Range_allocator &ram_alloc);


		/********************************************
		 ** Platform-implemented support functions **
		 ********************************************/

		/* FIXME Could this be merged with Dataspace::unmap() and friends? */

		/**
		 * Map region locally and return local base address
		 *
		 * Both parameters - base and size - must be page-aligned.
		 */
		addr_t _map_local(addr_t base, size_t size);

		/**
		 * Unmap Core-local mapping of region
		 *
		 * Both parameters - base and size - must be page-aligned.
		 */
		void _unmap_local(addr_t virt_base, size_t size, addr_t phys_base);

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
		Io_mem_session_component(Range_allocator &io_mem_alloc,
		                         Range_allocator &ram_alloc,
		                         Rpc_entrypoint  &ds_ep,
		                         const char      *args);

		/**
		 * Destructor
		 */
		~Io_mem_session_component();


		/******************************
		 ** Io-mem session interface **
		 ******************************/

		Io_mem_dataspace_capability dataspace() override { return _ds_cap; }
};

#endif /* _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_ */
