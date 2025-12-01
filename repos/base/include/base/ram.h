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
		Ram_quota const needed_ram  { align_addr(size, AT_PAGE) };
		Cap_quota const needed_caps { 1 };

		return _ram_guard.reserve(needed_ram).convert<Result>(
			[&] (Ram_quota_guard::Reservation &reserved_ram) {
				return _cap_guard.reserve(needed_caps).convert<Result>(
					[&] (Cap_quota_guard::Reservation &reserved_caps) {
						return _alloc.try_alloc(reserved_ram.amount, cache)
							.template convert<Result>(
								[&] (Allocation &allocation) -> Result {
									allocation.deallocate    = false;
									reserved_ram.deallocate  = false;
									reserved_caps.deallocate = false;
									return { *this, { allocation.cap,
									                  allocation.num_bytes } };
								},
								[&] (Error e) { return e; });
					}, [&] (Cap_quota_guard::Error) { return Error::OUT_OF_CAPS; }
				);
			}, [&] (Ram_quota_guard::Error) { return Error::OUT_OF_RAM; }
		);
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
