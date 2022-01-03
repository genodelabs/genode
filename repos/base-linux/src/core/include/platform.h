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

/* Genode includes */
#include <base/allocator_avl.h>
#include <util/arg_string.h>

/* core-local includes */
#include <platform_generic.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <synced_range_allocator.h>
#include <assertion.h>


/**
 * List of Unix environment variables, initialized by the startup code
 */
extern char **lx_environ;


/**
 * Read environment variable as long value
 */
static unsigned long ram_quota_from_env()
{
	using namespace Genode;

	for (char **curr = lx_environ; curr && *curr; curr++) {

		Arg arg = Arg_string::find_arg(*curr, "GENODE_RAM_QUOTA");
		if (arg.valid())
			return arg.ulong_value(~0);
	}

	return ~0;
}


class Genode::Platform : public Platform_generic
{
	private:

		/**
		 * Allocator for core-internal meta data
		 */
		Synced_range_allocator<Allocator_avl> _core_mem_alloc;

		Rom_fs _dummy_rom_fs { };

		struct Dummy_allocator : Range_allocator
		{
			void         free(void *, size_t)          override { ASSERT_NEVER_CALLED; }
			bool         need_size_for_free()    const override { ASSERT_NEVER_CALLED; }
			size_t       consumed()              const override { ASSERT_NEVER_CALLED; }
			size_t       overhead(size_t)        const override { ASSERT_NEVER_CALLED; }
			Range_result add_range   (addr_t, size_t ) override { ASSERT_NEVER_CALLED; }
			Range_result remove_range(addr_t, size_t ) override { ASSERT_NEVER_CALLED; }
			void         free(void *)                  override { ASSERT_NEVER_CALLED; }
			size_t       avail()                 const override { ASSERT_NEVER_CALLED; }
			bool         valid_addr(addr_t )     const override { ASSERT_NEVER_CALLED; }
			Alloc_result try_alloc(size_t)             override { ASSERT_NEVER_CALLED; }
			Alloc_result alloc_addr(size_t, addr_t)    override { ASSERT_NEVER_CALLED; }

			Alloc_result alloc_aligned(size_t, unsigned, Range) override
			{ ASSERT_NEVER_CALLED; }


		} _dummy_alloc { };

		/**
		 * Allocator for pseudo physical memory
		 */
		struct Pseudo_ram_allocator : Range_allocator
		{
			Alloc_result try_alloc(size_t) override
			{
				return nullptr;
			}

			Alloc_result alloc_aligned(size_t, unsigned, Range) override
			{
				return nullptr;
			}

			Alloc_result alloc_addr(size_t, addr_t) override
			{
				return nullptr;
			}

			Range_result add_range(addr_t, size_t)    override
			{
				return Range_ok();
			}

			Range_result remove_range(addr_t, size_t) override
			{
				return Range_ok();
			}

			void   free(void *)                 override { }
			void   free(void *, size_t)         override { }
			size_t avail()                const override { return ram_quota_from_env(); }
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
		size_t max_caps() const override { return 20000; }

		void wait_for_exit() override;
};

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
