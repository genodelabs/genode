/*
 * \brief  Quota-bounds-checking implementation of the 'Ram_allocator'
 *         interface specifically for core
 * \author Norman Feske
 * \date   2017-05-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_CONSTRAINED_CORE_RAM_H_
#define _CORE__INCLUDE__CORE_CONSTRAINED_CORE_RAM_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <types.h>

namespace Core { class Constrained_core_ram; }


class Core::Constrained_core_ram : public Allocator
{
	private:

		Ram_quota_guard &_ram_guard;
		Cap_quota_guard &_cap_guard;
		Range_allocator &_core_mem;

		Genode::uint64_t core_mem_allocated { 0 };

	public:

		Constrained_core_ram(Ram_quota_guard &ram_guard,
		                     Cap_quota_guard &cap_guard,
		                     Range_allocator &core_mem)
		:
			_ram_guard(ram_guard), _cap_guard(cap_guard), _core_mem(core_mem)
		{ }

		~Constrained_core_ram()
		{
			if (!core_mem_allocated)
				return;

			error(this, " memory leaking of size ", core_mem_allocated,
			      " in core !");
		}

		Alloc_result try_alloc(size_t const size) override
		{
			size_t const page_aligned_size = align_addr(size, 12);

			Ram_quota_guard::Reservation ram (_ram_guard,
			                                  Ram_quota{page_aligned_size});
			/* on some kernels we require a cap, on some not XXX */
			Cap_quota_guard::Reservation caps(_cap_guard, Cap_quota{1});

			return _core_mem.try_alloc(page_aligned_size).convert<Alloc_result>(

				[&] (void *ptr) {
					ram.acknowledge();
					caps.acknowledge();
					core_mem_allocated += page_aligned_size;
					return ptr; },

				[&] (Alloc_error error) {
					return error; });
		}

		void free(void *ptr, size_t const size) override
		{
			size_t const page_aligned_size = align_addr(size, 12);

			_core_mem.free(ptr, page_aligned_size);

			_ram_guard.replenish(Ram_quota{page_aligned_size});
			/* on some kernels we require a cap, on some not XXX */
			_cap_guard.replenish(Cap_quota{1});

			core_mem_allocated -= page_aligned_size;
		}

		size_t consumed()           const override { return (size_t)core_mem_allocated; }
		size_t overhead(size_t)     const override { return 0; }
		bool   need_size_for_free() const override { return true; }
};
#endif /* _CORE__INCLUDE__CORE_CONSTRAINED_CORE_RAM_H_ */
