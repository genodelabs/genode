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
#include <assertion.h>

namespace Genode {
	class Platform;
	template <auto> class Static_allocator;
	class Address_space;
}


/**
 * Allocator operating on a static memory pool
 *
 * \param MAX   maximum number of 4096 blocks
 *
 * The size of a single ELEM must be a multiple of sizeof(long).
 */
template <auto MAX>
class Genode::Static_allocator : public Allocator
{
	private:

		Bit_allocator<MAX> _used { };

		struct Elem_space { uint8_t space[4096]; };

		Elem_space _elements[MAX];

	public:

		class Alloc_failed { };

		Alloc_result try_alloc(size_t size) override
		{
			if (size > sizeof(Elem_space)) {
				error("unexpected allocation size of ", size);
				return Alloc_error::DENIED;
			}

			try {
				return &_elements[_used.alloc()]; }
			catch (typename Bit_allocator<MAX>::Out_of_indices) {
				return Alloc_error::DENIED; }
		}

		size_t overhead(size_t) const override { return 0; }

		void free(void *ptr, size_t) override
		{
			Elem_space *elem = reinterpret_cast<Elem_space *>(ptr);
			unsigned const index = (unsigned)(elem - &_elements[0]);
			_used.free(index);
		}

		bool need_size_for_free() const override { return false; }
};

class Genode::Platform : public Platform_generic
{
	private:

		Core_mem_allocator _core_mem_alloc { }; /* core-accessible memory */
		Phys_allocator     _io_mem_alloc;       /* MMIO allocator         */
		Phys_allocator     _io_port_alloc;      /* I/O port allocator     */
		Phys_allocator     _irq_alloc;          /* IRQ allocator          */

		Initial_untyped_pool _initial_untyped_pool { };

		/*
		 * Allocator for tracking unused physical addresses, which is used
		 * to allocate a range within the phys CNode for ROM modules.
		 */
		Phys_allocator _unused_phys_alloc;

		/*
		 * Allocator for tracking unused virtual addresses, which are not
		 * backed by page tables.
		 */
		Phys_allocator _unused_virt_alloc;

		void       _init_unused_phys_alloc();
		bool const _init_unused_phys_alloc_done;

		Rom_fs _rom_fs { };  /* ROM file system */

		/*
		 * Virtual address range usable by non-core processes
		 */
		addr_t _vm_base = 0;
		size_t _vm_size = 0;

		/*
		 * Until this point, no interaction with the seL4 kernel was needed.
		 * However, the next steps involve the invokation of system calls and
		 * the use of kernel services. To use the kernel bindings, we first
		 * need to initialize the TLS mechanism that is used to find the IPC
		 * buffer for the calling thread.
		 */
		void init_sel4_ipc_buffer();
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

		/* allocate 2nd-level CNode for storing cap selectors for untyped 4k objects */
		Cnode _untyped_cnode { Cap_sel(seL4_CapInitThreadCNode),
		                       Cnode_index(Core_cspace::untyped_cnode_4k()),
		                       Core_cspace::NUM_PHYS_SEL_LOG2, _initial_untyped_pool };

		/* allocate 2nd-level CNode for storing cap selectors for untyped 16k objects */
		Cnode _untyped_cnode_16k { Cap_sel(seL4_CapInitThreadCNode),
		                           Cnode_index(Core_cspace::untyped_cnode_16k()),
		                           Core_cspace::NUM_PHYS_SEL_LOG2, _initial_untyped_pool };

		/*
		 * XXX Consider making Bit_allocator::_reserve public so that we can
		 *     turn the bit allocator into a private member of 'Core_sel_alloc'.
		 */
		typedef Bit_allocator<1 << Core_cspace::NUM_CORE_SEL_LOG2> Core_sel_bit_alloc;

		struct Core_sel_alloc : Cap_sel_alloc, private Core_sel_bit_alloc
		{
			Mutex _mutex { };

			Core_sel_alloc() { _reserve(0, Core_cspace::core_static_sel_end()); }

			Cap_sel alloc() override
			{
				Mutex::Guard guard(_mutex);

				try {
					return Cap_sel((uint32_t)Core_sel_bit_alloc::alloc()); }
				catch (Bit_allocator::Out_of_indices) {
					throw Alloc_failed(); }
			}

			void free(Cap_sel sel) override
			{
				Mutex::Guard guard(_mutex);

				Core_sel_bit_alloc::free(sel.value());
			}

		} _core_sel_alloc { };

		/**
		 * Replace initial CSpace with custom CSpace layout
		 */
		void       _switch_to_core_cspace();
		bool const _switch_to_core_cspace_done;

		Static_allocator<sizeof(void *) * 6> _core_page_table_registry_alloc { };
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
		Range_allocator &_phys_alloc = _core_mem_alloc.phys_alloc();

		/**
		 * Initialize core allocators
		 */
		void       _init_allocators();
		bool const _init_allocators_done;

		Vm_space _core_vm_space;

		void _init_rom_modules();

		/**
		 * Unmap page frame provided by kernel during early bootup.
		 */
		long _unmap_page_frame(Cap_sel const &);

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

		Affinity::Space affinity_space() const override {
			return (unsigned)sel4_boot_info().numNodes; }

		bool supports_direct_unmap() const override { return true; }

		Address_space &core_pd() { ASSERT_NEVER_CALLED; }


		/*******************
		 ** seL4 specific **
		 *******************/

		Cnode &phys_cnode() { return _phys_cnode; }
		Cnode &top_cnode()  { return _top_cnode; }
		Cnode &core_cnode() { return _core_cnode; }

		Vm_space &core_vm_space() { return _core_vm_space; }

		Cap_sel_alloc &core_sel_alloc() { return _core_sel_alloc; }
		unsigned alloc_core_rcv_sel();

		void reset_sel(unsigned sel);

		Cap_sel asid_pool() const { return _asid_pool_sel; }

		void wait_for_exit() override;

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

		size_t max_caps() const override
		{
			return 1UL << Core_cspace::NUM_CORE_SEL_LOG2;
		}

		bool core_needs_platform_pd() const override { return false; }
};

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
