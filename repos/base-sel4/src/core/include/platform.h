/*
 * \brief  Platform interface
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

/* local includes */
#include <platform_generic.h>
#include <core_mem_alloc.h>
#include <vm_space.h>
#include <core_cspace.h>
#include <initial_untyped_pool.h>

namespace Genode { class Platform; }


class Genode::Platform : public Platform_generic
{
	private:

		Core_mem_allocator _core_mem_alloc; /* core-accessible memory */
		Phys_allocator     _io_mem_alloc;   /* MMIO allocator         */
		Phys_allocator     _io_port_alloc;  /* I/O port allocator     */
		Phys_allocator     _irq_alloc;      /* IRQ allocator          */

		Initial_untyped_pool _initial_untyped_pool;

		/*
		 * Allocator for tracking unused physical addresses, which is used
		 * to allocate a range within the phys CNode for ROM modules.
		 */
		Phys_allocator _unused_phys_alloc;

		void       _init_unused_phys_alloc();
		bool const _init_unused_phys_alloc_done;

		Rom_fs _rom_fs;  /* ROM file system */


		/**
		 * Virtual address range usable by non-core processes
		 */
		addr_t _vm_base;
		size_t _vm_size;

		/*
		 * Until this point, no interaction with the seL4 kernel was needed.
		 * However, the next steps involve the invokation of system calls and
		 * the use of kernel services. To use the kernel bindings, we first
		 * need to initialize the TLS mechanism that is used to find the IPC
		 * buffer for the calling thread.
		 */
		bool const _init_sel4_ipc_buffer_done;

		/* allocate 1st-level CNode */
		Cnode _top_cnode { Cap_sel(seL4_CapInitThreadCNode),
		                   Cnode_index(Core_cspace::top_cnode_sel()),
		                   Core_cspace::NUM_TOP_SEL_LOG2,
		                   _initial_untyped_pool };

		/* allocate 2nd-level CNode to align core's CNode with the LSB of the CSpace*/
		Cnode _core_pad_cnode { Cap_sel(seL4_CapInitThreadCNode),
		                        Cnode_index(Core_cspace::core_pad_cnode_sel()),
		                        Core_cspace::NUM_CORE_PAD_SEL_LOG2,
		                        _initial_untyped_pool };

		/* allocate 3rd-level CNode for core's objects */
		Cnode _core_cnode { Cap_sel(seL4_CapInitThreadCNode),
		                    Cnode_index(Core_cspace::core_cnode_sel()),
		                    Core_cspace::NUM_CORE_SEL_LOG2, _initial_untyped_pool };

		/* allocate 2nd-level CNode for storing page-frame cap selectors */
		Cnode _phys_cnode { Cap_sel(seL4_CapInitThreadCNode),
		                    Cnode_index(Core_cspace::phys_cnode_sel()),
		                    Core_cspace::NUM_PHYS_SEL_LOG2, _initial_untyped_pool };

		/* allocate 2nd-level CNode for storing cap selectors for untyped pages */
		Cnode _untyped_cnode { Cap_sel(seL4_CapInitThreadCNode),
		                       Cnode_index(Core_cspace::untyped_cnode_sel()),
		                       Core_cspace::NUM_PHYS_SEL_LOG2, _initial_untyped_pool };

		/*
		 * XXX Consider making Bit_allocator::_reserve public so that we can
		 *     turn the bit allocator into a private member of 'Core_sel_alloc'.
		 */
		typedef Bit_allocator<1 << Core_cspace::NUM_PHYS_SEL_LOG2> Core_sel_bit_alloc;

		struct Core_sel_alloc : Cap_sel_alloc, private Core_sel_bit_alloc
		{
			Lock _lock;

			Core_sel_alloc() { _reserve(0, Core_cspace::core_static_sel_end()); }

			Cap_sel alloc() override
			{
				Lock::Guard guard(_lock);

				try {
					return Cap_sel(Core_sel_bit_alloc::alloc()); }
				catch (Bit_allocator::Out_of_indices) {
					throw Alloc_failed(); }
			}

			void free(Cap_sel sel) override
			{
				Lock::Guard guard(_lock);

				Core_sel_bit_alloc::free(sel.value());
			}

		} _core_sel_alloc;

		/**
		 * Replace initial CSpace with custom CSpace layout
		 */
		void       _switch_to_core_cspace();
		bool const _switch_to_core_cspace_done;

		Page_table_registry _core_page_table_registry;

		/**
		 * Pre-populate core's '_page_table_registry' with the information
		 * about the initial page tables and page frames as set up by the
		 * kernel
		 */
		void       _init_core_page_table_registry();
		bool const _init_core_page_table_registry_done;

		Cap_sel _init_asid_pool();
		Cap_sel const _asid_pool_sel = _init_asid_pool();

		/**
		 * Shortcut for physical memory allocator
		 */
		Range_allocator &_phys_alloc = *_core_mem_alloc.phys_alloc();

		/**
		 * Initialize core allocators
		 */
		void       _init_allocators();
		bool const _init_allocators_done;

		Vm_space _core_vm_space;

		void _init_rom_modules();

	public:

		/**
		 * Constructor
		 */
		Platform();


		/********************************
		 ** Generic platform interface **
		 ********************************/

		Range_allocator *ram_alloc()      { return  _core_mem_alloc.phys_alloc(); }
		Range_allocator *io_mem_alloc()   { return &_io_mem_alloc; }
		Range_allocator *io_port_alloc()  { return &_io_port_alloc; }
		Range_allocator *irq_alloc()      { return &_irq_alloc; }
		Range_allocator *region_alloc()   { return  _core_mem_alloc.virt_alloc(); }
		Range_allocator *core_mem_alloc() { return &_core_mem_alloc; }
		addr_t           vm_start() const { return _vm_base; }
		size_t           vm_size()  const { return _vm_size;  }
		Rom_fs          *rom_fs()         { return &_rom_fs; }

		Cnode &phys_cnode() { return _phys_cnode; }
		Cnode &top_cnode()  { return _top_cnode; }
		Cnode &core_cnode() { return _core_cnode; }

		Vm_space &core_vm_space() { return _core_vm_space; }

		Cap_sel_alloc &core_sel_alloc() { return _core_sel_alloc; }
		unsigned alloc_core_rcv_sel();

		void reset_sel(unsigned sel);

		Cap_sel asid_pool() const { return _asid_pool_sel; }

		void wait_for_exit();

		/**
		 * Determine size of a core local mapping required for a
		 * core_rm_session detach().
		 */
		size_t region_alloc_size_at(void * addr) {
			return (*_core_mem_alloc.virt_alloc())()->size_at(addr); }

};

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
