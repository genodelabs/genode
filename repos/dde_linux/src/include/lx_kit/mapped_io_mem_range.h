/*
 * \brief  Representation of a locally-mapped MMIO range
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__MAPPED_IO_MEM_RANGE_H_
#define _LX_KIT__MAPPED_IO_MEM_RANGE_H_

/* Genode includes */
#include <dataspace/capability.h>

/* Linux emulation environment includes */
#include <lx_kit/types.h>

namespace Lx {

	using namespace Genode;

	void *ioremap(addr_t, unsigned long, Cache_attribute);
	void  iounmap(volatile void*);
	Dataspace_capability ioremap_lookup(addr_t, size_t);
}


#endif /* _LX_KIT__MAPPED_IO_MEM_RANGE_H_ */
