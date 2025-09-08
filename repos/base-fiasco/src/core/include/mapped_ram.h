/*
 * \brief  Page-granular allocator for core-private RAM
 * \author Norman Feske
 * \date   2025-09-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__MAPPED_RAM_H_
#define _CORE__MAPPED_RAM_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <types.h>

namespace Core { struct Mapped_ram_allocator; }


struct Core::Mapped_ram_allocator
{
	private:

		Range_allocator &_phys;
		Range_allocator &_virt;

	public:

		struct Attr
		{
			size_t num_pages;
			addr_t phys;
			addr_t virt;

			size_t num_bytes() const { return num_pages*get_page_size(); }
			void * ptr()             { return (void *)virt; }
		};

		enum class Error { DENIED };

		using Allocation = Genode::Allocation<Mapped_ram_allocator>;
		using Result     = Allocation::Attempt;

		Mapped_ram_allocator(Range_allocator &phys, Range_allocator &virt)
		: _phys(phys), _virt(virt) { }

		struct Align { uint8_t log2; };

		Result alloc(size_t, Align);

		void _free(Allocation &);
};

#endif /* _CORE__MAPPED_RAM_H_ */
