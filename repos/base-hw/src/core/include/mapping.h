/*
 * \brief  Representation of physical to virtual memory mappings
 * \author Stefan Kalkowski
 * \date   2016-11-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MAPPING_H_
#define _MAPPING_H_

#include <memory_region.h>
#include <page_flags.h>

namespace Genode { class Mapping; }

class Genode::Mapping
{
	private:

		Memory_region _phys { 0, 0 };
		addr_t        _virt = 0;
		Page_flags    _flags { RO, NO_EXEC, KERN, NO_GLOBAL, RAM, CACHED };

	public:

		Mapping() {}

		Mapping(addr_t phys, addr_t virt, size_t size, Page_flags flags)
		: _phys(phys, size), _virt(virt), _flags(flags) {}

		void print(Output & out) const
		{
			Genode::print(out, "physical region(", _phys,
			              ") => virtual address=", (void*) _virt,
			              " with page-flags: ", _flags, ")");
		}

		addr_t     phys()  const { return _phys.base; }
		addr_t     virt()  const { return _virt;      }
		size_t     size()  const { return _phys.size; }
		Page_flags flags() const { return _flags;     }


		/***********************************************
		 ** Interface used by generic region_map code **
		 ***********************************************/

		Mapping(addr_t virt, addr_t phys, Cache_attribute cacheable,
				bool io, unsigned size_log2, bool writeable)
			: _phys(phys, 1 << size_log2), _virt(virt),
			  _flags { writeable ? RW : RO, EXEC, USER, NO_GLOBAL,
			           io ? DEVICE : RAM, cacheable } {}

		void prepare_map_operation() const {}
};

#endif /* _MAPPING_H_ */
