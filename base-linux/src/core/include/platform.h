/*
 * \brief  Linux platform
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2007-09-10
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__PLATFORM_H_
#define _CORE__INCLUDE__LINUX__PLATFORM_H_

#include <base/sync_allocator.h>
#include <base/allocator_avl.h>
#include <base/lock_guard.h>

#include <platform_generic.h>
#include <platform_pd.h>
#include <platform_thread.h>

namespace Genode {

	using namespace Genode;

	class Platform : public Platform_generic
	{
		private:

			Synchronized_range_allocator<Allocator_avl> _ram_alloc;  /* RAM allocator */

		public:

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator *core_mem_alloc() { return &_ram_alloc; }
			Range_allocator *ram_alloc()      { return &_ram_alloc; }
			Range_allocator *io_mem_alloc()   { return 0; }
			Range_allocator *io_port_alloc()  { return 0; }
			Range_allocator *irq_alloc()      { return 0; }
			Range_allocator *region_alloc()   { return 0; }
			addr_t           vm_start() const { return 0; }
			size_t           vm_size()  const { return 0; }
			Rom_fs          *rom_fs()         { return 0; }

			void wait_for_exit();
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
