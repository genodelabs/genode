/*
 * \brief  Component-local region-map implementation based on mmap
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__REGION_MAP_MMAP_H_
#define _INCLUDE__BASE__INTERNAL__REGION_MAP_MMAP_H_

/* Genode includes */
#include <base/env.h>
#include <region_map/region_map.h>
#include <dataspace/client.h>

/* base-internal includes */
#include <base/internal/local_capability.h>
#include <base/internal/region_registry.h>

namespace Genode { class Region_map_mmap; }


/**
 * On Linux, we use a locally implemented region map that attaches dataspaces
 * via mmap to the local address space.
 */
class Genode::Region_map_mmap : public Region_map, public Dataspace
{
	private:

		Region_registry _rmap;
		bool      const _sub_rm;  /* false if region map is root */
		size_t    const _size;

		/**
		 * Base offset of the RM session
		 *
		 * For a normal RM session (the one that comes with the 'env()', this
		 * value is zero. If the RM session is used as nested dataspace,
		 * '_base' contains the address where the managed dataspace is attached
		 * in the root RM session.
		 *
		 * Note that a managed dataspace cannot be attached more than once.
		 * Furthermore, managed dataspace cannot be attached to another managed
		 * dataspace. The nested dataspace emulation is solely implemented to
		 * support the common use case of managed dataspaces as mechanism to
		 * reserve parts of the local address space from being populated by the
		 * 'env()->rm_session()'. (i.e., for the stack area, or for the
		 * placement of consecutive shared-library segments)
		 */
		addr_t _base;

		bool _is_attached() const { return _base > 0; }

		void _add_to_rmap(Region const &);

		/**
		 * Reserve VM region for sub-rm dataspace
		 */
		addr_t _reserve_local(bool   use_local_addr,
		                      addr_t local_addr,
		                      size_t size);

		/**
		 * Map dataspace into local address space
		 */
		void *_map_local(Dataspace_capability ds,
		                 size_t               size,
		                 addr_t               offset,
		                 bool                 use_local_addr,
		                 addr_t               local_addr,
		                 bool                 executable,
		                 bool                 overmap = false);

		/**
		 * Determine size of dataspace
		 *
		 * For core, this function performs a local lookup of the
		 * 'Dataspace_component' object. For non-core programs, the dataspace
		 * size is determined via an RPC to core (calling 'Dataspace::size()').
		 */
		size_t _dataspace_size(Capability<Dataspace>);

		/**
		 * Determine file descriptor of dataspace
		 */
		int _dataspace_fd(Capability<Dataspace>);

		/**
		 * Determine whether dataspace is writable
		 */
		bool _dataspace_writable(Capability<Dataspace>);

	public:

		Region_map_mmap(bool sub_rm, size_t size = ~0)
		: _sub_rm(sub_rm), _size(size), _base(0) { }

		~Region_map_mmap()
		{
			/* detach sub RM session when destructed */
			if (_sub_rm && _is_attached())
				env_deprecated()->rm_session()->detach((void *)_base);
		}


		/**************************
		 ** Region map interface **
		 **************************/

		Local_addr attach(Dataspace_capability ds, size_t size,
		                  off_t, bool, Local_addr, bool executable);

		void detach(Local_addr local_addr);

		void fault_handler(Signal_context_capability handler) { }

		State state() { return State(); }


		/*************************
		 ** Dataspace interface **
		 *************************/

		size_t size() { return _size; }

		addr_t phys_addr() { return 0; }

		bool writable() { return true; }

		/**
		 * Return pseudo dataspace capability of the RM session
		 *
		 * The capability returned by this function is only usable
		 * as argument to 'Region_map_mmap::attach'. It is not a
		 * real capability.
		 */
		Dataspace_capability dataspace() {
			return Local_capability<Dataspace>::local_cap(this); }
};

#endif /* _INCLUDE__BASE__INTERNAL__REGION_MAP_MMAP_H_ */
