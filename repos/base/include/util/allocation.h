/*
 * \brief  Common result type for allocators
 * \author Norman Feske
 * \date   2025-04-04
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__ALLOCATION_H_
#define _INCLUDE__UTIL__ALLOCATION_H_

#include <util/attempt.h>

namespace Genode { template <typename> class Allocation; }


/**
 * Representation of an allocation
 *
 * The 'Allocation' base class provides a guard mechanism for reverting the
 * allocation at destruction time of an 'Allocation' object, unless the
 * 'deallocate' member is manually set to false. The 'ALLOCATOR' is expected
 * to implement a '_free' method performing the deallocation.
 *
 * An 'Allocation' object holds allocator-type-specific attributes ('Attr')
 * that are directly accessible in the scope of the 'Allocation' object.
 */
template <typename ALLOC>
class Genode::Allocation : Noncopyable, public ALLOC::Attr
{
	private:

		ALLOC &_alloc;

	public:

		using Attr  = typename ALLOC::Attr;
		using Error = typename ALLOC::Error;

		bool deallocate = true;

		Allocation(ALLOC &alloc, Attr attr) : Attr(attr), _alloc(alloc) { }

		~Allocation() { if (deallocate) _alloc._free(*this); }

		struct Attempt;
};


/**
 * Result type for allocators reflecting error conditions
 *
 * The 'Allocator::Attempt' type is suitable 'Result' type for allocators that
 * either return an 'Allocation' or an 'Error'. The 'Attempt' type has
 * unique-pointer semantics. It cannot be copied. But it can be reassigned.
 */
template <typename ALLOC>
struct Genode::Allocation<ALLOC>::Attempt : Unique_attempt<Allocation, Error>
{
	Attempt(ALLOC &alloc, Attr a) : Unique_attempt<Allocation, Error>(alloc, a) { }
	Attempt(Error e)              : Unique_attempt<Allocation, Error>(e)        { }
	Attempt()                     : Unique_attempt<Allocation, Error>(Error {}) { }

	Attempt &operator = (Attempt &&other)
	{
		/* take over the ownership of the allocation from 'other' */
		other.with_result(
			[&] (Allocation &a) { this->_construct(a._alloc, a); a.deallocate = false; },
			[&] (Error e)       { this->_destruct(e); });

		other._destruct({});
		return *this;
	}
};


#endif /* _INCLUDE__UTIL__ALLOCATION_H_ */
