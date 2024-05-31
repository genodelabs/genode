/*
 * \brief  Broadwell type definitions
 * \author Josef Soentgen
 * \data   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <base/ram_allocator.h>

namespace Igd {

	using Ram = Genode::Ram_dataspace_capability;

	using uint8_t  = Genode::uint8_t;
	using uint16_t = Genode::uint16_t;
	using uint32_t = Genode::uint32_t;
	using uint64_t = Genode::uint64_t;
	using addr_t   = Genode::addr_t;
	using size_t   = Genode::size_t;

	enum {
		PAGE_SIZE = 4096,
	};

	inline void wmb() { asm volatile ("sfence": : :"memory"); }
}

#endif /* _TYPES_H_ */
