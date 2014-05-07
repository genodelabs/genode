/*
 * \brief  Platform interface
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-12-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

/* Genode includes */
#include <base/sync_allocator.h>
#include <base/allocator_avl.h>
#include <irq_session/irq_session.h>

/* base-hw includes */
#include <kernel/log.h>
#include <kernel/core_interface.h>

/* core includes */
#include <platform_generic.h>
#include <core_rm_session.h>
#include <core_mem_alloc.h>

namespace Genode {

	/**
	 * Manages all platform ressources
	 */
	class Platform : public Platform_generic
	{
		typedef Core_mem_allocator::Phys_allocator Phys_allocator;

		Core_mem_allocator _core_mem_alloc; /* core-accessible memory */
		Phys_allocator     _io_mem_alloc;   /* MMIO allocator         */
		Phys_allocator     _irq_alloc;      /* IRQ allocator          */
		Rom_fs             _rom_fs;         /* ROM file system        */

		/*
		 * Virtual-memory range for non-core address spaces.
		 * The virtual memory layout of core is maintained in
		 * '_core_mem_alloc.virt_alloc()'.
		 */
		addr_t             _vm_start;
		size_t             _vm_size;


		public:

		/**
		 * Get one of the consecutively numbered available resource regions
		 *
		 * \return  >0  region pointer if region with index 'i' exists
		 *          0   if region with index 'i' doesn't exist
		 *
		 * These functions should provide all ressources that are available
		 * on the current platform.
		 */
		static Native_region * _ram_regions(unsigned i);
		static Native_region * _mmio_regions(unsigned i);

		/**
		 * Get one of the consecutively numbered core regions
		 *
		 * \return  >0  Region pointer if region with index 'i' exists
		 *          0   If region with index 'i' doesn't exist
		 *
		 * Core regions are address regions that must be permitted to
		 * core only, such as the core image ROM. These regions are normally
		 * a subset of the ressource regions provided above.
		 */
		static Native_region * _core_only_ram_regions(unsigned i);
		static Native_region * _core_only_mmio_regions(unsigned i);

		/**
		 * Get one of the consecutively numbered user interrupts
		 *
		 * \param i  index of interrupt
		 *
		 * \return  >0  pointer to the name of the requested interrupt
		 *          0   no interrupt for that index
		 */
		static unsigned * _irq(unsigned const i);

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Platform_generic interface **
			 ********************************/

			inline Range_allocator * core_mem_alloc() {
				return &_core_mem_alloc; }

			inline Range_allocator * ram_alloc() {
				return _core_mem_alloc.phys_alloc(); }

			inline Range_allocator * region_alloc() {
				return _core_mem_alloc.virt_alloc(); }

			inline Range_allocator * io_mem_alloc() { return &_io_mem_alloc; }

			inline Range_allocator * io_port_alloc() { return 0; }

			inline Range_allocator * irq_alloc() { return &_irq_alloc; }

			inline addr_t vm_start() const { return _vm_start; }

			inline size_t vm_size() const { return _vm_size; }

			inline Rom_fs *rom_fs() { return &_rom_fs; }

			inline void wait_for_exit()
			{
				while (1) { Kernel::pause_current_thread(); }
			};

			bool supports_direct_unmap() const { return 1; }

			Affinity::Space affinity_space() const
			{
				return Affinity::Space(PROCESSORS);
			}
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */

