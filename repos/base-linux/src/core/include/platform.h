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
#include <assertion.h>

namespace Genode {

	using namespace Genode;

	class Platform : public Platform_generic
	{
		private:

			/**
			 * Allocator for core-internal meta data
			 */
			Synced_range_allocator<Allocator_avl> _core_mem_alloc;

			Rom_fs _dummy_rom_fs { };

			struct Dummy_allocator : Range_allocator
			{
				void   free(void *, size_t)          override { ASSERT_NEVER_CALLED; }
				bool   need_size_for_free()    const override { ASSERT_NEVER_CALLED; }
				size_t consumed()              const override { ASSERT_NEVER_CALLED; }
				size_t overhead(size_t)        const override { ASSERT_NEVER_CALLED; }
				int    add_range   (addr_t, size_t ) override { ASSERT_NEVER_CALLED; }
				int    remove_range(addr_t, size_t ) override { ASSERT_NEVER_CALLED; }
				void   free(void *)                  override { ASSERT_NEVER_CALLED; }
				size_t avail()                 const override { ASSERT_NEVER_CALLED; }
				bool   valid_addr(addr_t )     const override { ASSERT_NEVER_CALLED; }
				bool   alloc(size_t, void **)        override { ASSERT_NEVER_CALLED; }

				Alloc_return alloc_aligned(size_t, void **, int, addr_t, addr_t) override
				{ ASSERT_NEVER_CALLED; }

				Alloc_return alloc_addr(size_t, addr_t) override
				{ ASSERT_NEVER_CALLED; }

			} _dummy_alloc { };

			/**
			 * Allocator for pseudo physical memory
			 */
			struct Pseudo_ram_allocator : Range_allocator
			{
				bool alloc(size_t, void **out_addr) override
				{
					*out_addr = 0;
					return true;
				}

				Alloc_return alloc_aligned(size_t, void **out_addr, int,
				                           addr_t, addr_t) override
				{
					*out_addr = 0;
					return Alloc_return::OK;
				}

				Alloc_return alloc_addr(size_t, addr_t) override
				{
					return Alloc_return::OK;
				}

				int    add_range(addr_t, size_t)    override { return 0; }
				int    remove_range(addr_t, size_t) override { return 0; }
				void   free(void *)                 override { }
				void   free(void *, size_t)         override { }
				size_t avail()                const override { return ~0; }
				bool   valid_addr(addr_t)     const override { return true; }
				size_t overhead(size_t)       const override { return 0; }
				bool   need_size_for_free()   const override { return true; }

			} _ram_alloc { };

		public:

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator &core_mem_alloc() override { return _core_mem_alloc; }
			Range_allocator &ram_alloc()      override { return _ram_alloc; }
			Range_allocator &io_mem_alloc()   override { return _dummy_alloc; }
			Range_allocator &io_port_alloc()  override { return _dummy_alloc; }
			Range_allocator &irq_alloc()      override { return _dummy_alloc; }
			Range_allocator &region_alloc()   override { return _dummy_alloc; }
			addr_t           vm_start() const override { return 0; }
			size_t           vm_size()  const override { return 0; }
			Rom_fs          &rom_fs()         override { return _dummy_rom_fs; }

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

			void wait_for_exit() override;
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
