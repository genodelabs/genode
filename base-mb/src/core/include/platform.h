/*
 * \brief  Platform interface
 * \author Martin Stein
 * \date   2010-09-08
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__INCLUDE__PLATFORM_H_
#define _SRC__CORE__INCLUDE__PLATFORM_H_

#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <base/sync_allocator.h>
#include <base/allocator_avl.h>
#include <platform_generic.h>
#include <kernel/syscalls.h>
#include <platform_pd.h>
#include <cpu/config.h>


namespace Roottask
{
	enum { 
		PAGER_TID      = User::MIN_THREAD_ID,

		CONTEXT_PAGE_SIZE_LOG2 = Kernel::Utcb::ALIGNMENT_LOG2,
		CONTEXT_PAGE_SIZE      = 1<<CONTEXT_PAGE_SIZE_LOG2,

		STACK_SIZE   = Cpu::_4KB_SIZE,
		CONTEXT_SIZE = STACK_SIZE + sizeof(Genode::Thread_base::Context),
	};

	/**
	 * The 'Platform_pd' instance according to roottask
	 */
	Genode::Platform_pd* platform_pd();

	/**
	 * Baseaddress of the downward directed physical stack and the 
	 * immediately following upward directed misc area belonging to 
	 * a specific thread within the roottask
	 */
	Genode::Thread_base::Context * physical_context(Genode::Native_thread_id tid);
}


namespace Genode {

	class Platform : public Platform_generic
	{
		private:

			typedef Synchronized_range_allocator<Allocator_avl> Phys_allocator;

			/*
			 * Core is mapped 1-to-1 physical-to-virtual except for the thread
			 * context area.  mapping out of context area. So a single memory
			 * allocator suffices for both, assigning physical RAM to
			 * dataspaces and allocating core-local memory.
			 */
			Phys_allocator  _core_mem_alloc; /* core-accessible memory */
			Phys_allocator  _io_mem_alloc;   /* MMIO allocator         */
			Phys_allocator  _io_port_alloc;  /* I/O port allocator     */
			Phys_allocator  _irq_alloc;      /* IRQ allocator          */
			Rom_fs          _rom_fs;         /* ROM file system        */

			/**
			 * Virtual address range usable by non-core processes
			 */
			addr_t _vm_base;
			size_t _vm_size;

			void _optimize_init_img_rom(long int & base, size_t const & size);

		public:

			virtual ~Platform() {}

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			inline Range_allocator *ram_alloc()      { return &_core_mem_alloc; }
			inline Range_allocator *io_mem_alloc()   { return &_io_mem_alloc; }
			inline Range_allocator *io_port_alloc()  { return &_io_port_alloc; }
			inline Range_allocator *irq_alloc()      { return &_irq_alloc; }
			inline Range_allocator *region_alloc()   { return 0; }

			/**
			 * We need a 'Range_allocator' instead of 'Allocator' as in 
			 * 'Platform_generic' to allocate aligned space for e.g. UTCB's
			 */
			inline Range_allocator *core_mem_alloc() { return &_core_mem_alloc; }

			inline addr_t vm_start() const { return _vm_base; }
			inline size_t vm_size()  const { return _vm_size; }

			inline Rom_fs *rom_fs() { return &_rom_fs; }

			inline void wait_for_exit() { sleep_forever(); }

	};
}

#endif /* _SRC__CORE__INCLUDE__PLATFORM_H_ */
