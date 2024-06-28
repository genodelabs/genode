/*
 * \brief  Interface for allocating RAM dataspaces
 * \author Norman Feske
 * \date   2017-05-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RAM_ALLOCATOR_H_
#define _INCLUDE__BASE__RAM_ALLOCATOR_H_

#include <util/attempt.h>
#include <base/capability.h>
#include <base/quota_guard.h>
#include <base/cache.h>
#include <dataspace/dataspace.h>

namespace Genode {

	struct Ram_dataspace : Dataspace { };

	using Ram_dataspace_capability = Capability<Ram_dataspace>;

	struct Ram_allocator;

	class Constrained_ram_allocator;
}


struct Genode::Ram_allocator : Interface
{
	enum class Alloc_error { OUT_OF_RAM, OUT_OF_CAPS, DENIED };

	using Alloc_result = Attempt<Ram_dataspace_capability, Alloc_error>;

	struct Denied : Exception { };

	/**
	 * Allocate RAM dataspace
	 *
	 * \param  size   size of RAM dataspace
	 * \param  cache  selects cacheability attributes of the memory,
	 *                uncached memory, i.e., for DMA buffers
	 *
	 * \return capability to RAM dataspace, or error code of type 'Alloc_error'
	 */
	virtual Alloc_result try_alloc(size_t size, Cache cache = CACHED) = 0;

	/**
	 * Allocate RAM dataspace
	 *
	 * \param  size   size of RAM dataspace
	 * \param  cache  selects cacheability attributes of the memory,
	 *                uncached memory, i.e., for DMA buffers
	 *
	 * \throw  Out_of_ram
	 * \throw  Out_of_caps
	 * \throw  Denied
	 *
	 * \return capability to new RAM dataspace
	 */
	Ram_dataspace_capability alloc(size_t size, Cache cache = CACHED)
	{
		return try_alloc(size, cache).convert<Ram_dataspace_capability>(
			[&] (Ram_dataspace_capability cap) {
				return cap; },

			[&] (Alloc_error error) -> Ram_dataspace_capability {
				switch (error) {
				case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
				case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
				case Alloc_error::DENIED:      break;
				}
				throw Denied();
			});
	}

	/**
	 * Free RAM dataspace
	 *
	 * \param ds  dataspace capability as returned by alloc
	 */
	virtual void free(Ram_dataspace_capability ds) = 0;

	/**
	 * Return size of dataspace in bytes
	 */
	virtual size_t dataspace_size(Ram_dataspace_capability) const = 0;
};


/**
 * Quota-bounds-checking wrapper of the 'Ram_allocator' interface
 */
class Genode::Constrained_ram_allocator : public Ram_allocator
{
	private:

		Ram_allocator   &_ram_alloc;
		Ram_quota_guard &_ram_guard;
		Cap_quota_guard &_cap_guard;

	public:

		Constrained_ram_allocator(Ram_allocator   &ram_alloc,
		                          Ram_quota_guard &ram_guard,
		                          Cap_quota_guard &cap_guard)
		:
			_ram_alloc(ram_alloc), _ram_guard(ram_guard), _cap_guard(cap_guard)
		{ }

		Alloc_result try_alloc(size_t size, Cache cache = CACHED) override
		{
			using Result = Alloc_result;

			size_t const page_aligned_size = align_addr(size, 12);

			Ram_quota const needed_ram  { page_aligned_size };
			Cap_quota const needed_caps { 1 };

			return _ram_guard.with_reservation<Result>(needed_ram,

				[&] (Reservation &ram_reservation) {

					return _cap_guard.with_reservation<Result>(needed_caps,

						[&] (Reservation &cap_reservation) -> Result {

							return _ram_alloc.try_alloc(page_aligned_size, cache)
							                 .convert<Result>(

								[&] (Ram_dataspace_capability ds) -> Result {
									return ds; },

								[&] (Alloc_error error) {
									cap_reservation.cancel();
									ram_reservation.cancel();
									return error; }
							);
						},
						[&] () -> Result {
							ram_reservation.cancel();
							return Alloc_error::OUT_OF_CAPS;
						}
					);
				},
				[&] () -> Result {
					return Alloc_error::OUT_OF_RAM; }
			);
		}

		void free(Ram_dataspace_capability ds) override
		{
			size_t const size = _ram_alloc.dataspace_size(ds);

			_ram_alloc.free(ds);

			_ram_guard.replenish(Ram_quota{size});
			_cap_guard.replenish(Cap_quota{1});
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override
		{
			return _ram_alloc.dataspace_size(ds);
		}
};

#endif /* _INCLUDE__BASE__RAM_ALLOCATOR_H_ */
