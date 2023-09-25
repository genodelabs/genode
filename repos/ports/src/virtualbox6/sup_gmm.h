/*
 * \brief  Suplib GMM implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-11-09
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_GMM_H_
#define _SUP_GMM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_dataspace.h>
#include <base/mutex.h>
#include <rm_session/connection.h>
#include <vm_session/connection.h>
#include <region_map/client.h>
#include <libc/allocator.h>

/* local includes */
#include <sup.h>


namespace Sup { struct Gmm; }

/**
 * Global (guest-memory) manager (GMM)
 *
 * Layers in this pool are (top-down)
 *
 * - Page-sized allocation from Allocator_avl
 * - Linear map of 32 GiB in VMM address space (Vmm_addr)
 * - VirtualBox GMM chunks of GMM_CHUNK_SIZE (2 MiB) filled with pages and
 *   referenced with PAGEIDs (offset in linear area)
 * - Slices of 128 MiB RAM dataspaces as backing store
 *
 * Notes
 *
 * - expect that guest-physical address not required here
 * - NIL_GMM_CHUNKID kept unused - so offset 0 is chunk ID 1
 * - we have to allocate from pool - so we need an allocator
 */
class Sup::Gmm
{
	public:

		struct Vmm_addr   { addr_t value; };
		struct Guest_addr { addr_t value; };
		struct Offset     { addr_t value; };
		struct Bytes      { size_t value; };
		struct Pages      { size_t value; };
		struct Page_id    { addr_t value; }; /* CHUNKID | PAGEIDX */

		struct Protection
		{
			bool readable;
			bool writeable;
			bool executable;

			bool none() const
			{
				return !readable && ! writeable && !executable;
			}

			bool operator == (Protection const &other) const
			{
				return readable   == other.readable
				    && writeable  == other.writeable
				    && executable == other.executable;
			}

			void print(Output &o) const
			{
				Genode::print(o, readable   ? "r" : "-",
				                 writeable  ? "w" : "-",
				                 executable ? "x" : "-");
			}
		};

	private:

		Env &_env;

		Vm_connection &_vm_connection;

		static constexpr Bytes _slice_size {     128*1024*1024ul };
		static constexpr Bytes _map_size   { 32*1024*1024*1024ul };
		static constexpr auto  _num_slices { _map_size.value / _slice_size.value };

		Mutex _mutex { };

		Dataspace_capability _slices[_num_slices];

		Pages _size_pages        { 0 }; /* current backing-store allocations */
		Pages _reservation_pages { 0 }; /* current VMMR0_DO_GMM_UPDATE_RESERVATION pages */
		Pages _alloc_ex_pages    { 0 }; /* current SUPPAGEALLOCEX pages */

		struct Map
		{
			Env &env;

			Bytes const size;

			Rm_connection connection { env };

			Region_map_client rm { connection.create(size.value) };

			Vmm_addr const base { (addr_t)env.rm().attach(rm.dataspace()) };
			Vmm_addr const end  { base.value + size.value - 1 };

			Map(Env &env, Bytes size) : env(env), size(size) { }

		} _map { _env, _map_size };

		void _add_one_slice();

		void _update_pool_size();

		Vmm_addr _alloc_pages(Pages);

		Offset _offset(Vmm_addr addr) const
		{
			if (addr.value < _map.base.value || addr.value > _map.end.value)
				throw Out_of_range();

			return Offset { addr.value - _map.base.value };
		}

		unsigned _slice_index(Offset offset) const
		{
			unsigned const index = offset.value / _slice_size.value;

			if (index > _num_slices)
				throw Out_of_range();

			return index;
		}

		unsigned _slice_index(Vmm_addr addr) const
		{
			return _slice_index(_offset(addr));
		}

		Libc::Allocator _md_alloc;
		Allocator_avl   _alloc { &_md_alloc };

	public:

		struct Out_of_range      : Exception { };
		struct Allocation_failed : Exception { };

		Gmm(Env &env, Vm_connection &);

		/**
		 * Extend pool according to VMMR0_DO_GMM_UPDATE_RESERVATION
		 */
		void reservation_pages(Pages);

		/**
		 * Allocate pages from reservation pool
		 */
		Vmm_addr alloc_from_reservation(Pages);

		/**
		 * Extend pool and allocate pages
		 */
		Vmm_addr alloc_ex(Pages);

		/**
		 * Free pages in pool
		 */
		void free(Vmm_addr);

		/**
		 * Get page ID for VMM address inside linear area
		 */
		Page_id page_id(Vmm_addr);

		/**
		 * Get 32-bit PAGEID from page ID
		 */
		static uint32_t page_id_as_uint32(Page_id);

		/**
		 * Get VMM address inside linear area for page ID
		 */
		Vmm_addr vmm_addr(Page_id);

		/**
		 * Make VMM memory available to the guest-physical address space
		 *
		 * \throw Out_of_range
		 */
		void map_to_guest(Vmm_addr, Guest_addr, Pages, Protection);
};

#endif /* _SUP_GMM_H_ */
