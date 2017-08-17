/*
 * \brief  Fiasco.OC platform
 * \author Christian Helmuth
 * \author Norman Feske
 * \author Stefan Kalkowski
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
#include <base/synced_allocator.h>
#include <base/allocator_avl.h>

/* Core includes */
#include <pager.h>
#include <cap_id_alloc.h>
#include <platform_generic.h>
#include <platform_thread.h>
#include <platform_pd.h>


namespace Genode {

	class Platform : public Platform_generic
	{
		private:

			/**
			 * Pager object representing the pager of core namely sigma0
			 */
			struct Sigma0 : public Pager_object
			{
				/**
				 * Constructor
				 */
				Sigma0(Cap_index*);

				int pager(Ipc_pager &ps) { /* never called */ return -1; }
			};

			/*
			 * Shortcut for the type of allocator instances for physical resources
			 */
			typedef Synced_range_allocator<Allocator_avl> Phys_allocator;

			Platform_pd     *_core_pd;        /* core protection domain object */
			Phys_allocator   _ram_alloc;      /* RAM allocator */
			Phys_allocator   _io_mem_alloc;   /* MMIO allocator */
			Phys_allocator   _io_port_alloc;  /* I/O port allocator */
			Phys_allocator   _irq_alloc;      /* IRQ allocator */
			Phys_allocator   _region_alloc;   /* virtual memory allocator for core */
			Cap_id_allocator _cap_id_alloc;   /* capability id allocator */
			Rom_fs           _rom_fs;         /* ROM file system */
			Rom_module       _kip_rom;        /* ROM module for Fiasco KIP */
			Sigma0           _sigma0;

			addr_t           _vm_start;       /* begin of virtual memory */
			size_t           _vm_size;        /* size of virtual memory */


			/*
			 * We do not export any boot module loaded before FIRST_ROM.
			 */
			enum { FIRST_ROM = 3 };

			/**
			 * Setup base resources
			 *
			 * - Map and provide KIP as ROM module
			 * - Initializes region allocator
			 */
			void _setup_basics();

			/**
			 * Setup RAM, IO_MEM, and region allocators
			 */
			void _setup_mem_alloc();

			/**
			 * Setup I/O port space allocator
			 */
			void _setup_io_port_alloc();

			/**
			 * Setup IRQ allocator
			 */
			void _setup_irq_alloc();

			/**
			 * Parse multi-boot information and update ROM database
			 */
			void _init_rom_modules();

			/**
			 * Setup pager for core-internal threads
			 */
			void _setup_core_pager();

			addr_t _rom_module_phys(addr_t virt) { return virt; }

		public:

			/**
			 * Core pager thread that handles core-internal page-faults
			 */
			struct Core_pager : public Platform_thread, public Pager_object
			{
				/**
				 * Constructor
				 */
				Core_pager(Platform_pd *core_pd, Sigma0*);

				int pager(Ipc_pager &ps) { /* never called */ return -1; }
			};

			/**
			 * Return singleton instance of core pager object
			 */
			Core_pager *core_pager();

			/**
			 * Set interrupt trigger/polarity (e.g., level or edge, high or low)
			 */
			static void setup_irq_mode(unsigned irq_number, unsigned trigger, 
			                           unsigned polarity);

			/**
			 * Constructor
			 */
			Platform();

			/**
			 * Accessor for core pd object
			 */
			Platform_pd *core_pd() { return _core_pd; }


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator  *core_mem_alloc() { return &_ram_alloc;     }
			Range_allocator  *ram_alloc()      { return &_ram_alloc;     }
			Range_allocator  *io_mem_alloc()   { return &_io_mem_alloc;  }
			Range_allocator  *io_port_alloc()  { return &_io_port_alloc; }
			Range_allocator  *irq_alloc()      { return &_irq_alloc;     }
			Range_allocator  *region_alloc()   { return &_region_alloc;  }
			Cap_id_allocator *cap_id_alloc()   { return &_cap_id_alloc;  }
			addr_t            vm_start() const { return _vm_start;       }
			size_t            vm_size()  const { return _vm_size;        }
			Rom_fs           *rom_fs()         { return &_rom_fs;        }
			Affinity::Space   affinity_space() const;

			size_t max_caps() const override { return cap_idx_alloc()->max_caps(); }

			void wait_for_exit();
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
