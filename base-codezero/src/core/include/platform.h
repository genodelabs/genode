/*
 * \brief  Platform interface
 * \author Norman Feske
 * \date   2009-10-02
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
#include <base/printf.h>

/* local includes */
#include <platform_generic.h>
#include <core_mem_alloc.h>

namespace Genode {

	class Platform : public Platform_generic
	{
		private:

			typedef Core_mem_allocator::Phys_allocator Phys_allocator;

			Core_mem_allocator _core_mem_alloc; /* core-accessible memory */
			Phys_allocator     _io_mem_alloc;   /* MMIO allocator         */
			Phys_allocator     _io_port_alloc;  /* I/O port allocator     */
			Phys_allocator     _irq_alloc;      /* IRQ allocator          */
			Rom_fs             _rom_fs;         /* ROM file system        */

			/**
			 * Virtual address range usable by non-core processes
			 */
			addr_t _vm_base;
			size_t _vm_size;

			int _init_rom_fs();

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

			void wait_for_exit();
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
