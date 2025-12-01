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
#include <map_local.h>
#include <types.h>

namespace Core { struct Mapped_ram_allocator; }


class Core::Mapped_ram_allocator
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

			size_t num_bytes() const { return num_pages*PAGE_SIZE; }
			void * ptr()       const { return (void *)virt; }
		};

		enum class Error { DENIED };

		using Allocation = Genode::Allocation<Mapped_ram_allocator>;
		using Result     = Allocation::Attempt;

		Mapped_ram_allocator(Range_allocator &phys, Range_allocator &virt)
		: _phys(phys), _virt(virt) { }

		Result alloc(size_t num_bytes, Align align)
		{
			size_t const page_rounded_size = align_addr(num_bytes, AT_PAGE);
			size_t const num_pages = page_rounded_size / PAGE_SIZE;

			align.log2 = max(align.log2, PAGE_SIZE_LOG2);

			/* allocate physical pages */
			return _phys.alloc_aligned(page_rounded_size, align).convert<Result>(

				[&] (Range_allocator::Allocation &phys) -> Result {

					/* allocate range in core's virtual address space */
					return _virt.alloc_aligned(page_rounded_size, align).convert<Result>(

						[&] (Range_allocator::Allocation &virt) -> Result {

							/* make physical page accessible at the designated virtual address */
							if (!map_local(addr_t(phys.ptr), addr_t(virt.ptr), num_pages)) {
								error("local map in core failed");
								return Error::DENIED;
							}

							phys.deallocate = false;
							virt.deallocate = false;

							memset(virt.ptr, 0, num_bytes);

							return { *this, { num_pages, addr_t(phys.ptr), addr_t(virt.ptr) } };
						},
						[&] (Alloc_error e) {
							error("could not allocate virtual address range in core of size ",
							      page_rounded_size, " (error ", (int)e, ")");
							return Error::DENIED;
						});
				},
				[&] (Alloc_error e) {
					error("could not allocate physical RAM region of size ",
					      page_rounded_size, " (error ", (int)e, ")");
					return Error::DENIED;
				});
		}

		void _free(Allocation &a)
		{
			unmap_local(a.virt, a.num_pages);

			_virt.free((void *)a.virt, a.num_bytes());
			_phys.free((void *)a.phys, a.num_bytes());
		}
};

#endif /* _CORE__MAPPED_RAM_H_ */
