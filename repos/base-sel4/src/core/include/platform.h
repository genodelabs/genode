/*
 * \brief  Platform interface
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

/* Genode includes */
#include <base/printf.h>

/* local includes */
#include <platform_generic.h>
#include <core_mem_alloc.h>
#include <vm_space.h>
#include <core_cspace.h>

namespace Genode { class Platform; }


class Genode::Platform : public Platform_generic
{
	private:

		Core_mem_allocator _core_mem_alloc; /* core-accessible memory */
		Phys_allocator     _io_mem_alloc;   /* MMIO allocator         */
		Phys_allocator     _io_port_alloc;  /* I/O port allocator     */
		Phys_allocator     _irq_alloc;      /* IRQ allocator          */

		/*
		 * Allocator for tracking unused physical addresses, which is used
		 * to allocate a range within the phys CNode for ROM modules.
		 */
		Phys_allocator _unused_phys_alloc;

		void       _init_unused_phys_alloc();
		bool const _init_unused_phys_alloc_done;

		Rom_fs _rom_fs;  /* ROM file system */

		/**
		 * Shortcut for physical memory allocator
		 */
		Range_allocator &_phys_alloc = *_core_mem_alloc.phys_alloc();

		/**
		 * Virtual address range usable by non-core processes
		 */
		addr_t _vm_base;
		size_t _vm_size;

		/**
		 * Initialize core allocators
		 */
		void       _init_allocators();
		bool const _init_allocators_done;

		/*
		 * Until this point, no interaction with the seL4 kernel was needed.
		 * However, the next steps involve the invokation of system calls and
		 * the use of kernel services. To use the kernel bindings, we first
		 * need to initialize the TLS mechanism that is used to find the IPC
		 * buffer for the calling thread.
		 */
		bool const _init_sel4_ipc_buffer_done;

		/* allocate 1st-level CNode */
		Cnode _top_cnode { seL4_CapInitThreadCNode, Core_cspace::TOP_CNODE_SEL,
		                   Core_cspace::NUM_TOP_SEL_LOG2, _phys_alloc };

		/* allocate 2nd-level CNode to align core's CNode with the LSB of the CSpace*/
		Cnode _core_pad_cnode { seL4_CapInitThreadCNode, Core_cspace::CORE_PAD_CNODE_SEL,
		                        Core_cspace::NUM_CORE_PAD_SEL_LOG2,
		                        _phys_alloc };

		/* allocate 3rd-level CNode for core's objects */
		Cnode _core_cnode { seL4_CapInitThreadCNode, Core_cspace::CORE_CNODE_SEL,
		                    Core_cspace::NUM_CORE_SEL_LOG2, _phys_alloc };

		/* allocate 2nd-level CNode for storing page-frame cap selectors */
		Cnode _phys_cnode { seL4_CapInitThreadCNode, Core_cspace::PHYS_CNODE_SEL,
		                    Core_cspace::NUM_PHYS_SEL_LOG2, _phys_alloc };

		struct Core_sel_alloc : Bit_allocator<1 << Core_cspace::NUM_PHYS_SEL_LOG2>
		{
			Core_sel_alloc() { _reserve(0, Core_cspace::CORE_STATIC_SEL_END); }
		} _core_sel_alloc;

		Lock _core_sel_alloc_lock;

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

		unsigned alloc_core_sel();
		unsigned alloc_core_rcv_sel();
		void reset_sel(unsigned sel);
		void free_core_sel(unsigned sel);

		void wait_for_exit();
};

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
