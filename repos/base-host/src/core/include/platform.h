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

/* core includes */
#include <platform_generic.h>

namespace Genode {

	class Platform : public Platform_generic
	{
		public:

			/**
			 * Constructor
			 */
			Platform();


			/********************************
			 ** Generic platform interface **
			 ********************************/

			Range_allocator *ram_alloc()      { return 0; }
			Range_allocator *io_mem_alloc()   { return 0; }
			Range_allocator *io_port_alloc()  { return 0; }
			Range_allocator *irq_alloc()      { return 0; }
			Range_allocator *region_alloc()   { return 0; }
			Range_allocator *core_mem_alloc() { return 0; }
			addr_t           vm_start() const { return 0; }
			size_t           vm_size()  const { return 0; }
			Rom_fs          *rom_fs()         { return 0; }

			void wait_for_exit();
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
