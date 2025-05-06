/*
 * \brief  Interfaces for byte-wise local memory allocations
 * \author Norman Feske
 * \date   2025-04-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__MEMORY_H_
#define _INCLUDE__BASE__MEMORY_H_

#include <base/error.h>
#include <util/allocation.h>
#include <util/construct_at.h>

namespace Genode::Memory {

	struct Constrained_allocator;

	template <typename> class Constrained_obj_allocator;
}


/**
 * Allocator of bytes that reflects allocation errors
 */
struct Genode::Memory::Constrained_allocator : Interface, Noncopyable
{
	struct Attr
	{
		void *ptr; size_t num_bytes;

		void print(Output &out) const
		{
			Genode::print(out, "ptr=", ptr, " num_bytes=", num_bytes);
		}
	};

	using Error      = Alloc_error;
	using Allocation = Genode::Allocation<Constrained_allocator>;
	using Result     = Allocation::Attempt;

	/**
	 * Allocate memory block
	 */
	virtual Result try_alloc(size_t num_bytes) = 0;

	/**
	 * Release allocation
	 *
	 * \noapi
	 */
	virtual void _free(Allocation &) = 0;
};


/**
 * Utility to allocate and construct objects of type 'T'
 *
 * This utility constructs an object on backing store allocated from a
 * constrained memory allocator.
 *
 * In constrast to the traditional 'new' operator, the 'create' method
 * reflects allocation errors as return values instead of exceptions.
 *
 * An object is destructed at deallocation time.
 *
 * In contrast the traditional 'delete' operator, which accepts the object
 * type of a base class of the allocated object as argument, the type for
 * the deallocation has to correspond to the allocated type.
 */
template <typename T>
class Genode::Memory::Constrained_obj_allocator : Noncopyable
{
	private:

		Constrained_allocator &_alloc;

	public:

		struct Attr { T &obj; };

		using Error      = Alloc_error;
		using Allocation = Genode::Allocation<Constrained_obj_allocator>;
		using Result     = Allocation::Attempt;

		Constrained_obj_allocator(Constrained_allocator &alloc) : _alloc(alloc) { }

		/**
		 * Allocate and construct object
		 *
		 * \param args  constructor arguments
		 */
		Result create(auto &&... args)
		{
			return _alloc.try_alloc(sizeof(T)).convert<Result>(
				[&] (Constrained_allocator::Allocation &bytes) -> Result {

					T *obj_ptr = construct_at<T>(bytes.ptr, args...);

					/* hand over ownership to caller of 'create' */
					bytes.deallocate = false;
					return { *this, { *obj_ptr } };
				},
				[&] (Alloc_error e) -> Result { return e; });
		}

		/**
		 * Destruct and deallocate object
		 */
		void destroy(T &obj)
		{
			/* call destructor */
			obj.~T();

			/* deallocate bytes via ~Allocation */
			Constrained_allocator::Allocation { _alloc, { &obj, sizeof(obj) } };
		}

		void _free(Allocation &a) { destroy(a.obj); }
};


namespace Genode::Memory {

	/* shortcut for the most commonly used type allocation */
	using Allocation = Constrained_allocator::Allocation;
}

#endif /* _INCLUDE__BASE__MEMORY_H_ */
