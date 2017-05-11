/*
 * \brief  Generic platform
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2007-09-10
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_GENERIC_H_
#define _CORE__INCLUDE__PLATFORM_GENERIC_H_

/* Genode includes */
#include <thread/capability.h>
#include <base/allocator.h>
#include <base/affinity.h>

/* core includes */
#include <rom_fs.h>

namespace Genode {

	/**
	 * Generic platform interface
	 */
	class Platform_generic
	{
		public:

			virtual ~Platform_generic() { }

			/**
			 * Allocator of core-local mapped virtual memory
			 */
			virtual Range_allocator *core_mem_alloc() = 0;

			/**
			 * Allocator of physical memory
			 */
			virtual Range_allocator *ram_alloc() = 0;

			/**
			 * Allocator of free address ranges within core
			 */
			virtual Range_allocator *region_alloc() = 0;

			/**
			 * I/O memory allocator
			 */
			virtual Range_allocator *io_mem_alloc() = 0;

			/**
			 * I/O port allocator
			 */
			virtual Range_allocator *io_port_alloc() = 0;

			/**
			 * IRQ allocator
			 */
			virtual Range_allocator *irq_alloc() = 0;

			/**
			 * Virtual memory configuration accessors
			 */
			virtual addr_t vm_start() const = 0;
			virtual size_t vm_size()  const = 0;

			/**
			 * ROM modules
			 */
			virtual Rom_fs *rom_fs() = 0;

			/**
			 * Wait for exit condition
			 */
			virtual void wait_for_exit() = 0;

			/**
			 * Return true if platform supports unmap
			 */
			virtual bool supports_unmap() { return true; }

			/**
			 * Return true if platform supports direct unmap (no mapping db)
			 */
			virtual bool supports_direct_unmap() const { return false; }

			/**
			 * Return number of physical CPUs present in the platform
			 *
			 * The default implementation returns a single CPU.
			 */
			virtual Affinity::Space affinity_space() const
			{
				return Affinity::Space(1);
			}

			/**
			 * Return system-wide maximum number of capabilities
			 */
			virtual size_t max_caps() const = 0;

			/**
			 * Return true if the core component relies on a 'Platform_pd' object
			 */
			virtual bool core_needs_platform_pd() const { return true; }
	};


	/**
	 * Request pointer to static generic platform interface of core
	 */
	extern Platform_generic *platform();

	class Platform;

	/**
	 * Access the platform-specific platform interface of core
	 *
	 * This function should only be called from platform-specific code.
	 */
	extern Platform *platform_specific();
}

#endif /* _CORE__INCLUDE__PLATFORM_GENERIC_H_ */
