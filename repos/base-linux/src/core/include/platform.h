/*
 * \brief  Linux platform
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2007-09-10
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

#include <base/allocator_avl.h>
#include <base/lock_guard.h>

#include <platform_generic.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <synced_range_allocator.h>

namespace Genode {

	using namespace Genode;

	class Platform : public Platform_generic
	{
		private:

			/**
			 * Allocator for core-internal meta data
			 */
			Synced_range_allocator<Allocator_avl> _core_mem_alloc;

			/**
			 * Allocator for pseudo physical memory
			 */
			struct Pseudo_ram_allocator : Range_allocator
			{
				bool alloc(size_t size, void **out_addr)
				{
					*out_addr = 0;
					return true;
				}

				Alloc_return alloc_aligned(size_t, void **out_addr, int,
				                           addr_t, addr_t)
				{
					*out_addr = 0;
					return Alloc_return::OK;
				}

				Alloc_return alloc_addr(size_t, addr_t)
				{
					return Alloc_return::OK;
				}

				int    add_range(addr_t, size_t)    override { return 0; }
				int    remove_range(addr_t, size_t) override { return 0; }
				void   free(void *) override                 { }
				void   free(void *, size_t) override         { }
				size_t avail() const override                { return ~0; }
				bool   valid_addr(addr_t) const override     { return true; }
				size_t overhead(size_t) const override       { return 0; }
				bool   need_size_for_free() const override   { return true; }
			};

			Pseudo_ram_allocator _ram_alloc;

		public:

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator *core_mem_alloc() override { return &_core_mem_alloc; }
			Range_allocator *ram_alloc()      override { return &_ram_alloc; }
			Range_allocator *io_mem_alloc()   override { return 0; }
			Range_allocator *io_port_alloc()  override { return 0; }
			Range_allocator *irq_alloc()      override { return 0; }
			Range_allocator *region_alloc()   override { return 0; }
			addr_t           vm_start() const override { return 0; }
			size_t           vm_size()  const override { return 0; }
			Rom_fs          *rom_fs()         override { return 0; }

			/*
			 * On Linux, the maximum number of capabilities is primarily
			 * constrained by the limited number of file descriptors within
			 * core. Each dataspace and and each thread consumes one
			 * descriptor. However, all capabilies managed by the same
			 * entrypoint share the same file descriptor such that the fd
			 * limit would be an overly pessimistic upper bound.
			 *
			 * Hence, we define the limit somewhat arbitrary on Linux and
			 * accept that scenarios may break when reaching core's fd limit.
			 */
			size_t max_caps() const override { return 10000; }

			void wait_for_exit();
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
