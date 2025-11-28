/*
 * \brief  RAM allocation utilities
 * \author Norman Feske
 * \date   2025-04-01
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_RAM_H_
#define _CORE__INCLUDE__CORE_RAM_H_

#include <base/mutex.h>

/* platform-specific core includes */
#include <mapped_ram.h>

namespace Core {

	struct Accounted_mapped_ram_allocator;

	template <typename> class Ram_obj_allocator;
}


struct Core::Accounted_mapped_ram_allocator
{
	Mutex                 _mutex { };
	Mapped_ram_allocator &_alloc;
	Ram_quota_guard      &_ram_guard;

	enum class Error { OUT_OF_RAM, DENIED };

	using Attr       = Mapped_ram_allocator::Attr;
	using Allocation = Genode::Allocation<Accounted_mapped_ram_allocator>;
	using Result     = Allocation::Attempt;
	using Align      = Mapped_ram_allocator::Align;

	Accounted_mapped_ram_allocator(Mapped_ram_allocator &alloc,
	                               Ram_quota_guard      &ram_guard)
	:
		_alloc(alloc), _ram_guard(ram_guard)
	{ }

	Result alloc(size_t num_bytes, Align align)
	{
		Mutex::Guard guard { _mutex };

		Ram_quota const needed_ram  { align_addr(num_bytes, 12) };

		return _ram_guard.reserve(needed_ram).convert<Result>(
			[&] (Ram_quota_guard::Reservation &reserved_ram) {
				return _alloc.alloc(reserved_ram.amount, align)
					.template convert<Result>(
						[&] (Mapped_ram_allocator::Allocation &allocation) -> Result {
							allocation.deallocate    = false;
							reserved_ram.deallocate  = false;
							return { *this, allocation };
						},
						[&] (Mapped_ram_allocator::Error) { return Error::DENIED; });
			}, [&] (Ram_quota_guard::Error) { return Error::OUT_OF_RAM; }
		);
	}

	void _free(Allocation &a)
	{
		Mutex::Guard guard { _mutex };

		/* deallocate via 'Mapped_ram_allocator::~Allocation' */
		{
			Mapped_ram_allocator::Allocation { _alloc, a };
		}

		_ram_guard.replenish(Ram_quota{a.num_bytes()});
	}
};


/**
 * Allocator for objects of type T that allocates each object on distinct RAM
 *
 * Since no two objects can share the same RAM page, the backing store of each
 * object can be released independently.
 */
template <typename T>
class Core::Ram_obj_allocator : Noncopyable
{
	private:

		Accounted_mapped_ram_allocator &_alloc;

	public:

		struct Attr
		{
			Accounted_mapped_ram_allocator::Allocation::Attr _attr;

			T &obj;
		};

		using Error      = Accounted_mapped_ram_allocator::Error;
		using Allocation = Genode::Allocation<Ram_obj_allocator>;
		using Result     = typename Allocation::Attempt;

		Ram_obj_allocator(Accounted_mapped_ram_allocator &alloc) : _alloc(alloc) { }

		/**
		 * Allocate and construct object
		 *
		 * \param args  constructor arguments
		 */
		Result create(auto &&... args)
		{
			return _alloc.alloc(sizeof(T),
			                    { .log2 = log2(alignof(T), 0u) }).convert<Result>(
				[&] (Accounted_mapped_ram_allocator::Allocation &a) -> Result {

					T *obj_ptr = construct_at<T>(a.ptr(), args...);

					/* hand over ownership to caller of 'create' */
					a.deallocate = false;
					return { *this, { a, *obj_ptr } };
				},
				[&] (Error e) -> Result { return e; });
		}

		void _free(Allocation &a)
		{
			a.obj.~T();

			/* deallocate via 'Accounted_mapped_ram_allocator::~Allocation' */
			Accounted_mapped_ram_allocator::Allocation { _alloc, a._attr };
		}
};

#endif /* _CORE__INCLUDE__CORE_RAM_H_ */
