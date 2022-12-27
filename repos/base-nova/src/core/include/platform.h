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
#include <address_space.h>

namespace Genode {

	class Platform : public Platform_generic
	{
		public:

			enum { MAX_SUPPORTED_CPUS = 64};

		private:

			Core_mem_allocator _core_mem_alloc  { };    /* core-accessible memory  */
			Phys_allocator     _io_mem_alloc;           /* MMIO allocator          */
			Phys_allocator     _io_port_alloc;          /* I/O port allocator      */
			Phys_allocator     _irq_alloc;              /* IRQ allocator           */
			Rom_fs             _rom_fs          { };    /* ROM file system         */
			unsigned           _gsi_base_sel    { 0 };  /* cap selector of 1st IRQ */
			unsigned           _core_pd_sel     { 0 };  /* cap selector of root PD */
			addr_t             _core_phys_start { 0 };

			/**
			 * Virtual address range usable by non-core processes
			 */
			const addr_t _vm_base;
			size_t _vm_size;

			/* available CPUs */
			Affinity::Space _cpus;

			/* map of virtual cpu ids in Genode to kernel cpu ids */
			uint8_t map_cpu_ids[MAX_SUPPORTED_CPUS];

			addr_t _map_pages(addr_t phys_page, addr_t pages,
			                  bool guard_page = false);

			size_t _max_caps = 0;

			void _init_rom_modules();

			addr_t _rom_module_phys(addr_t virt);

		public:

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator &ram_alloc()      override { return _core_mem_alloc.phys_alloc(); }
			Range_allocator &io_mem_alloc()   override { return _io_mem_alloc; }
			Range_allocator &io_port_alloc()  override { return _io_port_alloc; }
			Range_allocator &irq_alloc()      override { return _irq_alloc; }
			Range_allocator &region_alloc()   override { return _core_mem_alloc.virt_alloc(); }
			Range_allocator &core_mem_alloc() override { return _core_mem_alloc; }
			addr_t           vm_start() const override { return _vm_base; }
			size_t           vm_size()  const override { return _vm_size;  }
			Rom_fs          &rom_fs()         override { return _rom_fs; }
			size_t           max_caps() const override { return _max_caps; }
			void             wait_for_exit()  override;

			bool supports_direct_unmap() const override { return true; }

			Address_space &core_pd() { ASSERT_NEVER_CALLED; }

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
			size_t region_alloc_size_at(void * addr)
			{
				using Size_at_error = Allocator_avl::Size_at_error;

				return (_core_mem_alloc.virt_alloc())()->size_at(addr).convert<size_t>(
					[ ] (size_t s)      { return s;  },
					[ ] (Size_at_error) { return 0U; });
			}

			/**
			 * Return kernel CPU ID for given Genode CPU
			 */
			unsigned pager_index(Affinity::Location location) const;
			unsigned kernel_cpu_id(Affinity::Location location) const;

			Affinity::Location sanitize(Affinity::Location location) {
				return Affinity::Location(location.xpos() % _cpus.width(),
				                          location.ypos() % _cpus.height(),
				                          location.width(), location.height());
			}

			/**
			 * PD kernel capability selector of core
			 */
			unsigned core_pd_sel() const { return _core_pd_sel; }

			template <typename FUNC>
			void for_each_location(FUNC const &fn)
			{
				for (unsigned x = 0; x < _cpus.width(); x++) {
					for (unsigned y = 0; y < _cpus.height(); y++) {
						Affinity::Location location(x, y, 1, 1);
						fn(location);
					}
				}
			}
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
