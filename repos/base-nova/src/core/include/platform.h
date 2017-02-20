/*
 * \brief  Platform interface
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

/* core includes */
#include <platform_generic.h>
#include <core_mem_alloc.h>

namespace Genode {

	class Platform : public Platform_generic
	{
		public:

			enum { MAX_SUPPORTED_CPUS = 64};

		private:

			Core_mem_allocator _core_mem_alloc; /* core-accessible memory  */
			Phys_allocator     _io_mem_alloc;   /* MMIO allocator          */
			Phys_allocator     _io_port_alloc;  /* I/O port allocator      */
			Phys_allocator     _irq_alloc;      /* IRQ allocator           */
			Rom_fs             _rom_fs;         /* ROM file system         */
			unsigned           _gsi_base_sel;   /* cap selector of 1st IRQ */
			unsigned           _core_pd_sel;    /* cap selector of root PD */

			/**
			 * Virtual address range usable by non-core processes
			 */
			const addr_t _vm_base;
			size_t _vm_size;

			/* available CPUs */
			Affinity::Space _cpus;

			/* map of virtual cpu ids in Genode to kernel cpu ids */
			uint8_t map_cpu_ids[MAX_SUPPORTED_CPUS];

			addr_t _map_pages(addr_t phys_page, addr_t pages);

		public:

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator *ram_alloc()      override { return _core_mem_alloc.phys_alloc(); }
			Range_allocator *io_mem_alloc()   override { return &_io_mem_alloc; }
			Range_allocator *io_port_alloc()  override { return &_io_port_alloc; }
			Range_allocator *irq_alloc()      override { return &_irq_alloc; }
			Range_allocator *region_alloc()   override { return  _core_mem_alloc.virt_alloc(); }
			Range_allocator *core_mem_alloc() override { return &_core_mem_alloc; }
			addr_t           vm_start() const override { return _vm_base; }
			size_t           vm_size()  const override { return _vm_size;  }
			Rom_fs          *rom_fs()         override { return &_rom_fs; }

			void wait_for_exit() override;
			bool supports_unmap() override { return true; }
			bool supports_direct_unmap() const override { return true; }


			Affinity::Space affinity_space() const override { return _cpus; }


			/*******************
			 ** NOVA specific **
			 *******************/

			/**
			 * Return capability selector of first global system interrupt
			 */
			int gsi_base_sel() const { return _gsi_base_sel; }

			/**
			 * Determine size of a core local mapping required for a
			 * core_rm_session detach().
			 */
			size_t region_alloc_size_at(void * addr) {
				return (*_core_mem_alloc.virt_alloc())()->size_at(addr); }

			/**
			 * Return kernel CPU ID for given Genode CPU
			 */
			unsigned kernel_cpu_id(unsigned genode_cpu_id);

			/**
			 * PD kernel capability selector of core
			 */
			unsigned core_pd_sel() const { return _core_pd_sel; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
