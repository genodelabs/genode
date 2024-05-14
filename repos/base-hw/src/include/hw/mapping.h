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

#ifndef _SRC__LIB__HW__MAPPING_H_
#define _SRC__LIB__HW__MAPPING_H_

#include <hw/memory_region.h>
#include <cpu/page_flags.h>

namespace Hw {
	using Genode::Page_flags;
	using Genode::RO;
	using Genode::NO_EXEC;
	using Genode::KERN;
	using Genode::NO_GLOBAL;
	using Genode::RAM;

	class Mapping;
}



class Hw::Mapping
{
	protected:

		Memory_region _phys { 0, 0 };
		addr_t        _virt = 0;
		Page_flags    _flags { RO, NO_EXEC, KERN, NO_GLOBAL,
		                       RAM, Genode::CACHED };

	public:

		Mapping() { }

		Mapping(addr_t phys, addr_t virt, size_t size, Page_flags flags)
		: _phys(phys, size), _virt(virt), _flags(flags) { }

		void print(Genode::Output & out) const
		{
			Genode::print(out, "physical region(", _phys, ")",
			              " => virtual address=", (void*) _virt,
			              " with page-flags: ", _flags, ")");
		}

		addr_t     phys()  const { return _phys.base; }
		addr_t     virt()  const { return _virt;      }
		size_t     size()  const { return _phys.size; }
		Page_flags flags() const { return _flags;     }
};

#endif /* _SRC__LIB__HW__MAPPING_H_ */
