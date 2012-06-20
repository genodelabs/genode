/*
 * \brief  Core-internal dataspace representation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-20
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__DATASPACE_COMPONENT_H_
#define _CORE__INCLUDE__DATASPACE_COMPONENT_H_

/* Genode includes */
#include <base/printf.h>
#include <base/lock_guard.h>
#include <base/rpc_server.h>
#include <util/list.h>

/* core includes */
#include <util.h>

namespace Genode {

	class Rm_region;
	class Rm_session_component;

	/**
	 * Deriving classes can own a dataspace to implement conditional behavior
	 */
	class Dataspace_owner { };

	class Dataspace_component : public Rpc_object<Dataspace>
	{
		private:

			addr_t const _phys_addr;        /* address of dataspace in physical memory */
			addr_t       _core_local_addr;  /* address of core-local mapping           */
			size_t const _size;             /* size of dataspace in bytes              */
			bool   const _is_io_mem;        /* dataspace is I/O mem, not to be touched */
			bool   const _write_combined;   /* access I/O memory write-combined, or
			                                   RAM uncacheable respectively            */
			bool   const _writable;         /* false if dataspace is read-only         */

			List<Rm_region> _regions;    /* regions this is attached to */
			Lock            _lock;

			/* Holds the dataspace owner if a distinction between owner and
			 * others is necessary on the dataspace, otherwise it is 0 */
			Dataspace_owner const * _owner;

		protected:

			bool _managed;  /* true if this is a managed dataspace */

		private:

			/*
			 * Prevent copy-construction of objects with virtual functions.
			 */
			Dataspace_component(const Dataspace_component&);

		public:

			/**
			 * Default constructor returning an invalid dataspace
			 */
			Dataspace_component()
			:
				_phys_addr(0), _core_local_addr(0), _size(0),
				_is_io_mem(false), _write_combined(false), _writable(false),
				_owner(0), _managed(false) { }

			/**
			 * Constructor for non-I/O dataspaces
			 *
			 * This constructor is used by RAM and ROM dataspaces.
			 */
			Dataspace_component(size_t size, addr_t core_local_addr,
			                    bool write_combined, bool writable,
			                    Dataspace_owner *owner)
			:
				_phys_addr(core_local_addr), _core_local_addr(core_local_addr),
				_size(round_page(size)), _is_io_mem(false),
				_write_combined(write_combined), _writable(writable),
				_owner(owner), _managed(false) { }

			/**
			 * Constructor for dataspaces with different core-local and
			 * physical addresses
			 *
			 * This constructor is used by IO_MEM. Because I/O-memory areas may
			 * be located at addresses that are populated by data or text in
			 * Core's virtual address space, we need to map these areas to
			 * another core-local address. The local mapping in core's address
			 * space is needed to send a mapping to another address space.
			 */
			Dataspace_component(size_t size, addr_t core_local_addr,
			                    addr_t phys_addr, bool write_combined,
			                    bool writable, Dataspace_owner *owner)
			:
				_phys_addr(phys_addr), _core_local_addr(core_local_addr),
				_size(size), _is_io_mem(true), _write_combined(write_combined),
				_writable(writable), _owner(owner), _managed(false) { }

			/**
			 * Destructor
			 */
			~Dataspace_component();

			/**
			 * Return region-manager session corresponding to nested dataspace
			 *
			 * \retval  0  dataspace is not a nested dataspace
			 */
			virtual Rm_session_component *sub_rm_session() { return 0; }

			addr_t core_local_addr() const { return _core_local_addr; }
			bool is_io_mem()         const { return _is_io_mem; }
			bool write_combined()    const { return _write_combined; }

			/**
			 * Return dataspace base address to be used for map operations
			 *
			 * Depending on the used kernel, this may be a core-local address
			 * or a physical address.
			 */
			addr_t map_src_addr() const
			{
				return Genode::map_src_addr(_core_local_addr, _phys_addr);
			}

			void assign_core_local_addr(void *addr) { _core_local_addr = (addr_t)addr; }

			void attached_to(Rm_region *region);
			void detached_from(Rm_region *region);

			/**
			 * Check if dataspace is owned by a specific owner
			 */
			bool owner(Dataspace_owner * const o) const { return _owner == o; }

			List<Rm_region> *regions() { return &_regions; }

			/*************************
			 ** Dataspace interface **
			 *************************/

			size_t size()            { return _size; }
			addr_t phys_addr()       { return _phys_addr; }
			bool   writable()        { return _writable; }
			bool   is_managed()      { return _managed; }
	};
}

#endif /* _CORE__INCLUDE__DATASPACE_COMPONENT_H_ */
