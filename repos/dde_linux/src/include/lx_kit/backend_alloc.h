
/*
 * \brief  Back-end allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__BACKEND_ALLOC_H_
#define _LX_KIT__BACKEND_ALLOC_H_

/* Linux emulation environment includes */
#include <lx_kit/types.h>


namespace Lx {

	using namespace Genode;

	Ram_dataspace_capability backend_alloc(addr_t size,
	                                       Cache_attribute cached);
	void backend_free(Ram_dataspace_capability cap);
}

#endif /* _LX_KIT__BACKEND_ALLOC_H_ */
