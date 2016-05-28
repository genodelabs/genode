/*
 * \brief  OKL4 platform
 * \author Norman Feske
 * \date   2009-03-31
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

/* Genode includes */
#include <base/heap.h>

/* core includes */
#include <platform_generic.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <core_region_map.h>
#include <core_mem_alloc.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <bootinfo/bootinfo.h>
} }

namespace Genode {

	class Platform : public Platform_generic
	{
		private:

			using Rom_slab       = Tslab<Rom_module, get_page_size()>;
			using Thread_slab    = Tslab<Platform_thread, get_page_size()>;

			Platform_pd       *_core_pd;        /* core protection domain    */
			Platform_thread   *_core_pager;     /* pager for core threads    */
			Core_mem_allocator _core_mem_alloc; /* core-accessible memory    */
			Phys_allocator     _io_mem_alloc;   /* MMIO allocator            */
			Phys_allocator     _io_port_alloc;  /* I/O port allocator        */
			Phys_allocator     _irq_alloc;      /* IRQ allocator             */
			Rom_slab           _rom_slab;       /* Slab for rom modules      */
			Rom_fs             _rom_fs;         /* ROM file system           */
			Thread_slab        _thread_slab;    /* Slab for platform threads */

			/*
			 * Virtual-memory range for non-core address spaces.
			 * The virtual memory layout of core is maintained in
			 * '_core_mem_alloc.virt_alloc()'.
			 */
			addr_t             _vm_start;
			size_t             _vm_size;

			/*
			 * Start of address range used for the UTCBs
			 */
			addr_t             _utcb_base;

		public:

			/**
			 * Constructor
			 */
			Platform();

			/**
			 * Accessor for core pd object
			 */
			Platform_pd *core_pd() { return _core_pd; }

			/**
			 * Accessor for core pager thread object
			 */
			Platform_thread *core_pager() { return _core_pager; }

			/**
			 * Accessor for platform thread object slab allocator
			 */
			Thread_slab *thread_slab() { return &_thread_slab; }

			/**********************************************
			 ** Callbacks used for parsing the boot info **
			 **********************************************/

			static int bi_init_mem(Okl4::uintptr_t, Okl4::uintptr_t,
			                       Okl4::uintptr_t, Okl4::uintptr_t,
			                       const Okl4::bi_user_data_t *);

			static int bi_add_virt_mem(Okl4::bi_name_t,
			                           Okl4::uintptr_t, Okl4::uintptr_t,
			                           const Okl4::bi_user_data_t *);

			static int bi_add_phys_mem(Okl4::bi_name_t,
			                           Okl4::uintptr_t, Okl4::uintptr_t,
			                           const Okl4::bi_user_data_t *);

			static int bi_export_object(Okl4::bi_name_t, Okl4::bi_name_t,
			                            Okl4::bi_export_type_t, char *,
			                            Okl4::size_t,
			                            const Okl4::bi_user_data_t *);

			static Okl4::bi_name_t bi_new_ms(Okl4::bi_name_t,
			                                 Okl4::uintptr_t, Okl4::uintptr_t,
			                                 Okl4::uintptr_t, Okl4::uintptr_t,
			                                 Okl4::bi_name_t, Okl4::bi_name_t,
			                                 Okl4::bi_name_t,
			                                 const Okl4::bi_user_data_t *);

			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator *ram_alloc()      { return  _core_mem_alloc.phys_alloc(); }
			Range_allocator *io_mem_alloc()   { return &_io_mem_alloc; }
			Range_allocator *io_port_alloc()  { return &_io_port_alloc; }
			Range_allocator *irq_alloc()      { return &_irq_alloc; }
			Range_allocator *region_alloc()   { return  _core_mem_alloc.virt_alloc(); }
			Range_allocator *core_mem_alloc() { return &_core_mem_alloc; }
			addr_t           vm_start() const { return _vm_start; }
			size_t           vm_size()  const { return _vm_size; }
			Rom_fs          *rom_fs()         { return &_rom_fs; }

			void wait_for_exit();

			bool supports_direct_unmap() const { return true; }


			/**************************************
			 ** OKL4-specific platform interface **
			 **************************************/

			addr_t           utcb_base() { return _utcb_base; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
