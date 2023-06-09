/*
 * \brief  Core-internal dataspace representation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-20
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__DATASPACE_COMPONENT_H_
#define _CORE__INCLUDE__DATASPACE_COMPONENT_H_

/* Genode includes */
#include <base/mutex.h>
#include <base/rpc_server.h>
#include <util/list.h>

/* core includes */
#include <util.h>

namespace Core {

	class Rm_region;
	class Dataspace_component;

	/**
	 * Deriving classes can own a dataspace to implement conditional behavior
	 */
	class Dataspace_owner : Interface { };
}


class Core::Dataspace_component : public Rpc_object<Dataspace>
{
	public:

		struct Attr { addr_t base; size_t size; bool writeable; };

	private:

		addr_t const _phys_addr       = 0;  /* address of dataspace in physical memory */
		addr_t       _core_local_addr = 0;  /* address of core-local mapping           */
		size_t const _size            = 0;  /* size of dataspace in bytes              */
		bool   const _io_mem    = false;    /* dataspace is I/O mem, not to be touched */
		bool   const _writeable = false;    /* false if dataspace is read-only         */

		/*
		 * Access memory cached, write-combined, or uncached respectively
		 */
		Cache const _cache { CACHED };

		List<Rm_region> _regions { }; /* regions this is attached to */
		Mutex           _mutex   { };

		/*
		 * Holds the dataspace owner if a distinction between owner and
		 * others is necessary on the dataspace, otherwise it is 0.
		 */
		Dataspace_owner const * _owner = nullptr;

		/*
		 * Noncopyable
		 */
		Dataspace_component(Dataspace_component const &);
		Dataspace_component &operator = (Dataspace_component const &);

	protected:

		bool _managed = false;  /* true if this is a managed dataspace */

	public:

		/**
		 * Default constructor returning an invalid dataspace
		 */
		Dataspace_component() { }

		/**
		 * Constructor for non-I/O dataspaces
		 *
		 * This constructor is used by RAM and ROM dataspaces.
		 */
		Dataspace_component(size_t size, addr_t core_local_addr,
		                    Cache cache, bool writeable,
		                    Dataspace_owner *owner)
		:
			_phys_addr(core_local_addr), _core_local_addr(core_local_addr),
			_size(round_page(size)), _io_mem(false),
			_writeable(writeable), _cache(cache),
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
		                    addr_t phys_addr, Cache cache,
		                    bool writeable, Dataspace_owner *owner)
		:
			_phys_addr(phys_addr), _core_local_addr(core_local_addr),
			_size(size), _io_mem(true), _writeable(writeable),
			_cache(cache), _owner(owner), _managed(false) { }

		/**
		 * Destructor
		 */
		~Dataspace_component();

		/**
		 * Return region map corresponding to nested dataspace
		 *
		 * \retval  invalid capability if dataspace is not a nested one
		 */
		virtual Native_capability sub_rm() { return Dataspace_capability(); }

		addr_t core_local_addr() const { return _core_local_addr; }
		bool   io_mem()          const { return _io_mem; }
		Cache  cacheability()    const { return _cache; }
		addr_t phys_addr()       const { return _phys_addr; }
		bool   managed()         const { return _managed; }

		/**
		 * Return dataspace base address to be used for map operations
		 *
		 * Depending on the used kernel, this may be a core-local address
		 * or a physical address.
		 */
		addr_t map_src_addr() const
		{
			return Core::map_src_addr(_core_local_addr, _phys_addr);
		}

		Attr attr() const { return { .base      = map_src_addr(),
		                             .size      = _size,
		                             .writeable = _writeable }; }

		void assign_core_local_addr(void *addr) { _core_local_addr = (addr_t)addr; }

		void attached_to(Rm_region &region);
		void detached_from(Rm_region &region);

		/**
		 * Detach dataspace from all rm sessions.
		 */
		void detach_from_rm_sessions();

		/**
		 * Check if dataspace is owned by a specific owner
		 */
		bool owner(Dataspace_owner const &o) const { return _owner == &o; }

		List<Rm_region> &regions() { return _regions; }


		/*************************
		 ** Dataspace interface **
		 *************************/

		size_t size()      override { return _size; }
		bool   writeable() override { return _writeable; }


		void print(Output &out) const
		{
			addr_t const base = map_src_addr();
			Genode::print(out, "[", Hex(base), ",", Hex(base + _size - 1), "]");
		}
};

#endif /* _CORE__INCLUDE__DATASPACE_COMPONENT_H_ */
