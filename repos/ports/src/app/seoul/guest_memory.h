/*
 * \brief  Seoul Guest memory management
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GUEST_MEMORY_H_
#define _GUEST_MEMORY_H_

#include <rm_session/connection.h>
#include <vm_session/connection.h>
#include <region_map/client.h>

namespace Seoul { class Guest_memory; }

class Seoul::Guest_memory
{
	private:

		Genode::Env                      &_env;
		Genode::Vm_connection            &_vm_con;

		Genode::Ram_dataspace_capability  _ds;

		Genode::size_t const _backing_store_size;
		Genode::addr_t       _io_mem_alloc;
		Genode::addr_t const _local_addr;

		/*
		 * Noncopyable
		 */
		Guest_memory(Guest_memory const &);
		Guest_memory &operator = (Guest_memory const &);

		struct Region : Genode::List<Region>::Element {
			Genode::addr_t                _guest_addr;
			Genode::addr_t                _local_addr;
			Genode::Dataspace_capability  _ds;
			Genode::addr_t                _ds_size;

			Region (Genode::addr_t const guest_addr,
			        Genode::addr_t const local_addr,
			        Genode::Dataspace_capability ds,
			        Genode::addr_t const ds_size)
			: _guest_addr(guest_addr), _local_addr(local_addr),
			  _ds(ds), _ds_size(ds_size)
			{ }
		};

		Genode::List<Region> _regions { };

	public:

		/**
		 * Number of bytes that are available to the guest
		 *
		 * At startup time, some device models (i.e., the VGA controller) claim
		 * a bit of guest-physical memory for their respective devices (i.e.,
		 * the virtual frame buffer) by calling 'OP_ALLOC_FROM_GUEST'. This
		 * function allocates such blocks from the end of the backing store.
		 * The 'remaining_size' contains the number of bytes left at the lower
		 * part of the backing store for the use as  normal guest-physical RAM.
		 * It is initialized with the actual backing store size and then
		 * managed by the 'OP_ALLOC_FROM_GUEST' handler.
		 */
		Genode::size_t remaining_size;

		/**
		 * Constructor
		 *
		 * \param backing_store_size  number of bytes of physical RAM to be
		 *                            used as guest-physical and device memory,
		 *                            allocated from core's RAM service
		 */
		Guest_memory(Genode::Env &env, Genode::Allocator &alloc,
		             Genode::Vm_connection &vm_con,
		             Genode::size_t backing_store_size)
		:
			_env(env), _vm_con(vm_con),
			_ds(env.ram().alloc(backing_store_size)),
			_backing_store_size(backing_store_size),
			_io_mem_alloc(backing_store_size),
			_local_addr(env.rm().attach(_ds)),
			remaining_size(backing_store_size)
		{
			/* free region for attaching it executable */
			env.rm().detach(_local_addr);

			/*
			 * RAM used as backing store for guest-physical memory
			 */
			env.rm().attach_executable(_ds, _local_addr);

			/* register ds for VM region */
			add_region(alloc, 0UL, _local_addr, _ds, remaining_size);
		}

		~Guest_memory()
		{
			/* detach and free backing store */
			_env.rm().detach((void *)_local_addr);
			_env.ram().free(_ds);
		}

		/**
		 * Return pointer to locally mapped backing store
		 */
		char *backing_store_local_base()
		{
			return reinterpret_cast<char *>(_local_addr);
		}

		Genode::size_t backing_store_size()
		{
			return _backing_store_size;
		}

		void add_region(Genode::Allocator &alloc,
		                Genode::addr_t const guest_addr,
		                Genode::addr_t const local_addr,
		                Genode::Dataspace_capability ds,
		                Genode::addr_t const ds_size)
		{
			_regions.insert(new (alloc) Region(guest_addr, local_addr, ds, ds_size));
		}

		void attach_to_vm(Genode::Vm_connection &vm_con,
		                  Genode::addr_t guest_start,
		                  Genode::addr_t size)
		{
			for (Region *r = _regions.first(); r; r = r->next())
			{
				if (!r->_ds_size || !size || size & 0xfff) continue;
				if (size > r->_ds_size) continue;
				if (guest_start < r->_guest_addr) continue;
				if (guest_start > r->_guest_addr + r->_ds_size - 1) continue;

				Genode::addr_t ds_offset = guest_start - r->_guest_addr;
				vm_con.attach(r->_ds, guest_start, { .offset = ds_offset,
				                                     .size = size,
				                                     .executable = true,
				                                     .writeable = true });

				return;
			}
		}

		void detach(Genode::addr_t const guest_addr, Genode::addr_t const size)
		{
			_vm_con.detach(guest_addr, size);
		}

		Genode::addr_t alloc_io_memory(Genode::addr_t const size)
		{
			Genode::addr_t io_mem = _io_mem_alloc;
			_io_mem_alloc += size;
			return io_mem;
		}
};

#endif /* _GUEST_MEMORY_H_ */
