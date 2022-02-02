
/*
 * \brief  Back-end allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__BACKEND_ALLOC_H_
#define _LX_KIT__BACKEND_ALLOC_H_

/* Linux emulation environment includes */
#include <legacy/lx_kit/types.h>


namespace Lx {

	using namespace Genode;

	Ram_dataspace_capability backend_alloc(addr_t size, Cache);

	void backend_free(Ram_dataspace_capability cap);

	addr_t backend_dma_addr(Ram_dataspace_capability);
}

#endif /* _LX_KIT__BACKEND_ALLOC_H_ */
