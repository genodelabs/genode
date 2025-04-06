/*
 * \brief  Interfaces for allocating RAM
 * \author Norman Feske
 * \date   2025-04-03
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RAM_H_
#define _INCLUDE__BASE__RAM_H_

#include <util/allocation.h>
#include <base/error.h>
#include <base/capability.h>
#include <base/quota_guard.h>
#include <base/cache.h>
#include <dataspace/dataspace.h>

namespace Genode::Ram {

	struct Dataspace : Genode::Dataspace { };

	using Capability = Genode::Capability<Dataspace>;
	using Error      = Alloc_error;

	struct Constrained_allocator;

	template <typename> struct Accounted_allocator;
}


/**
 * Allocator of RAM inaccessible by the component at allocation time
 */
struct Genode::Ram::Constrained_allocator : Interface, Noncopyable
{
	struct Attr
	{
		Capability cap;
		size_t     num_bytes;
	};

	using Error      = Ram::Error;
	using Allocation = Genode::Allocation<Constrained_allocator>;
	using Result     = Allocation::Attempt;

	virtual Result try_alloc(size_t size, Cache cache = CACHED) = 0;

	/**
	 * Release allocation
	 *
	 * \noapi
	 */
	virtual void _free(Allocation &) = 0;
};


/**
 * Quota-bounds-checking wrapper of a constrained RAM allocator
 */
template <typename ALLOC>
struct Genode::Ram::Accounted_allocator : ALLOC
{
	ALLOC           &_alloc;
	Ram_quota_guard &_ram_guard;
	Cap_quota_guard &_cap_guard;

	using Allocation = typename ALLOC::Allocation;
	using Result     = typename ALLOC::Result;

	Accounted_allocator(ALLOC           &alloc,
	                    Ram_quota_guard &ram_guard,
	                    Cap_quota_guard &cap_guard)
	:
		_alloc(alloc), _ram_guard(ram_guard), _cap_guard(cap_guard)
	{ }

	Result try_alloc(size_t size, Cache cache = CACHED) override
	{
		size_t const page_aligned_size = align_addr(size, 12);

		Ram_quota const needed_ram  { page_aligned_size };
		Cap_quota const needed_caps { 1 };

		Capability cap   { };
		bool       ok    { };
		Error      error { };

		_ram_guard.with_reservation<void>(needed_ram,
			[&] (Reservation &ram_reservation) {
				_cap_guard.with_reservation<void>(needed_caps,
					[&] (Reservation &cap_reservation) {

						_alloc.try_alloc(page_aligned_size, cache).with_result(
							[&] (Allocation &allocation) {
								cap = allocation.cap;
								ok  = true;
								allocation.deallocate = false;
							},
							[&] (Error e) {
								cap_reservation.cancel();
								ram_reservation.cancel();
								error = e;
							});
					},
					[&] {
						ram_reservation.cancel();
						error = Error::OUT_OF_CAPS;
					}
				);
			},
			[&] { error = Error::OUT_OF_RAM; }
		);

		if (!ok)
			return error;

		return { *this, { cap, page_aligned_size } };
	}

	void _free(Allocation &allocation) override
	{
		_alloc._free(allocation);

		_ram_guard.replenish(Ram_quota{allocation.num_bytes});
		_cap_guard.replenish(Cap_quota{1});
	}
};


namespace Genode::Ram {

	/* shortcuts for the most commonly used type of allocator */
	using Allocator  = Constrained_allocator;
	using Allocation = Allocator::Allocation;
}

#endif /* _INCLUDE__BASE__RAM_H_ */
