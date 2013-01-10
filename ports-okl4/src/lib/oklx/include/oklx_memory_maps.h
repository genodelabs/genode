/*
 * \brief  OKLinux library specific memory data
 * \author Stefan Kalkowski
 * \date   2009-05-28
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OKLINUX_SUPPORT__INCLUDE__OKLX_MEMORY_MAPS_H_
#define _OKLINUX_SUPPORT__INCLUDE__OKLX_MEMORY_MAPS_H_

/* Genode includes */
#include <util/list.h>
#include <dataspace/client.h>

namespace Genode
{

	/**
	 * This class represents a memory area within the OKLinux kernel
	 * address space.
	 */
	class Memory_area : public List<Memory_area>::Element
	{
		private:

			addr_t _vaddr;             /* virtual address  */
			size_t _size;              /* memory area size */
			Dataspace_capability _cap; /* dataspace mapped */

		public:

			Memory_area(addr_t vaddr, size_t size, Dataspace_capability cap)
			: _vaddr(vaddr), _size(size), _cap(cap) {}

			/*
			 * Get the virtual address of the memory area
			 */
			addr_t vaddr() { return _vaddr; }

			/*
			 * Get the size of the memory area
			 */
			size_t size() { return _size; }

			/*
			 * Get the capability of the dataspace backing this memory area
			 */
			Dataspace_capability dataspace() { return _cap; }

			/*
			 * Get the physical address of this memory area
			 */
			addr_t paddr()
			{
				Dataspace_client dsc(_cap);
				return dsc.phys_addr();
			}

			/*
			 * List of all memory areas of the OKLinux kernel
			 */
			static List<Memory_area> *memory_map()
			{
				static List<Memory_area> _maps;
				return &_maps;
			}
	};
}

#endif //_OKLINUX_SUPPORT__INCLUDE__OKLX_MEMORY_MAPS_H_
