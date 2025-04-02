/*
 * \brief  Exception-based interface for allocating RAM dataspaces
 * \author Norman Feske
 * \date   2017-05-02
 */

/*
 * Copyright (C) 2017-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RAM_ALLOCATOR_H_
#define _INCLUDE__BASE__RAM_ALLOCATOR_H_

#include <base/ram.h>

namespace Genode {

	static inline void throw_exception(Ram::Error e) __attribute__((noreturn));

	struct Ram_allocator;

	/* type aliases used for API transition */
	using Ram_dataspace            = Ram::Dataspace;
	using Ram_dataspace_capability = Ram::Capability;
	using Accounted_ram_allocator  = Ram::Accounted_allocator<Ram_allocator>;
}


struct Genode::Ram_allocator : Ram::Unmapped_allocator
{
	struct Denied : Exception { };

	/**
	 * Allocate RAM
	 *
	 * \param  size   size of RAM dataspace
	 * \param  cache  selects cache attributes of the memory,
	 *                uncached memory, i.e., for DMA buffers
	 *
	 * \throw  Out_of_ram
	 * \throw  Out_of_caps
	 * \throw  Denied
	 *
	 * \return new RAM dataspace capability
	 */
	Ram::Capability alloc(size_t size, Cache cache = CACHED)
	{
		return try_alloc(size, cache).convert<Ram_dataspace_capability>(
			[&] (Allocation &a) { a.deallocate = false; return a.cap; },
			[&] (Ram::Error e) -> Ram::Capability { throw_exception(e); });
	}

	size_t _legacy_dataspace_size(Capability<Dataspace>);

	void free(Ram::Capability cap)
	{
		/*
		 * Deallocate via '~Ram::Allocation'.
		 *
		 * The real dataspace is merely needed for the quota tracking by
		 * 'Accounted_ram_allocator::_free'.
		 */
		Allocation { *this, { cap, _legacy_dataspace_size(cap) } };
	}

	void free(Ram::Capability cap, size_t size)
	{
		/* avoid call of '_legacy_dataspace_size' when size is known */
		Allocation { *this, { cap, size } };
	}

	/* type aliases used for API transition */
	using Alloc_result = Ram::Unmapped_allocator::Result;
	using Alloc_error  = Ram::Error;
};


namespace Genode {

	static inline void throw_exception(Ram::Error e)
	{
		switch (e) {
		case Ram::Error::OUT_OF_RAM:  throw Out_of_ram();
		case Ram::Error::OUT_OF_CAPS: throw Out_of_caps();
		case Ram::Error::DENIED:      break;
		}
		throw Ram_allocator::Denied();
	}
}

#endif /* _INCLUDE__BASE__RAM_ALLOCATOR_H_ */
