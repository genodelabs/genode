/*
 * \brief  Generic allocator interface
 * \author Norman Feske
 * \date   2006-04-16
 *
 * \deprecated  use 'Memory::Constrained_obj_allocator' instead on 'new'
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ALLOCATOR_H_
#define _INCLUDE__BASE__ALLOCATOR_H_

#include <util/interface.h>
#include <util/attempt.h>
#include <base/memory.h>
#include <base/stdint.h>
#include <base/exception.h>
#include <base/quota_guard.h>
#include <base/ram_allocator.h>

namespace Genode {

	struct Deallocator;
	struct Allocator;
	struct Range_allocator;

	template <typename T> void destroy(auto && dealloc, T *obj);
}


/**
 * Deallocator interface
 */
struct Genode::Deallocator : Interface
{
	/**
	 * Free block a previously allocated block
	 */
	virtual void free(void *addr, size_t size) = 0;

	/**
	 * Return true if the size argument of 'free' is required
	 *
	 * The generic 'Allocator' interface requires the caller of 'free'
	 * to supply a valid size argument but not all implementations make
	 * use of this argument. If this method returns false, it is safe
	 * to call 'free' with an invalid size.
	 *
	 * Allocators that rely on the size argument must not be used for
	 * constructing objects whose constructors may throw exceptions.
	 * See the documentation of 'operator delete(void *, Allocator *)'
	 * below for more details.
	 */
	virtual bool need_size_for_free() const = 0;
};


struct Genode::Allocator : Deallocator, Memory::Constrained_allocator
{
	using Out_of_memory = Out_of_ram;
	using Denied        = Genode::Denied;
	using Alloc_result  = Memory::Constrained_allocator::Result;
	using Allocation    = Memory::Allocation;

	/**
	 * Return total amount of backing store consumed by the allocator
	 */
	virtual size_t consumed() const { return 0; }

	/**
	 * Return meta-data overhead per block
	 */
	virtual size_t overhead(size_t size) const = 0;

	/**
	 * Raise exception according to the 'error' value
	 *
	 * \deprecated  use 'raise()' instead
	 * \noapi
	 */
	static void throw_alloc_error(Alloc_error e) __attribute__((noreturn))
	{
		raise(e);
	}

	/**
	 * Allocate block and signal error as an exception
	 *
	 * \param size  block size to allocate
	 *
	 * \throw       Out_of_ram
	 * \throw       Out_of_caps
	 * \throw       Denied
	 *
	 * \return      pointer to the new block
	 */
	void *alloc(size_t size)
	{
		return try_alloc(size).convert<void *>(
			[&] (Allocation &a) { a.deallocate = false; return a.ptr; },
			[&] (Alloc_error e) -> void * { raise(e); });
	}
};


struct Genode::Range_allocator : Allocator
{
	using Allocation = Allocator::Allocation;

	/**
	 * Return type of range-management operations
	 */
	using Range_result = Attempt<Ok, Alloc_error>;

	/**
	 * Add free address range to allocator
	 */
	virtual Range_result add_range(addr_t base, size_t size) = 0;

	/**
	 * Remove address range from allocator
	 */
	virtual Range_result remove_range(addr_t base, size_t size) = 0;

	struct Range { addr_t start, end; };

	/**
	 * Allocate block
	 *
	 * \param size   size of new block
	 * \param align  alignment of new block
	 * \param range  address-range constraint for the allocation
	 */
	virtual Result alloc_aligned(size_t size, Align align, Range range) = 0;

	/**
	 * Allocate block without constraining the address range
	 */
	Result alloc_aligned(size_t size, Align align)
	{
		return alloc_aligned(size, align, Range { .start = 0, .end = ~0UL });
	}

	/**
	 * Allocate block at address
	 *
	 * \param size   size of new block
	 * \param addr   desired address of block
	 */
	virtual Result alloc_addr(size_t size, addr_t addr) = 0;

	/**
	 * Free a previously allocated block
	 *
	 * NOTE: We have to declare the 'Allocator::free(void *)' method
	 * here as well to make the compiler happy. Otherwise the C++
	 * overload resolution would not find 'Allocator::free(void *)'.
	 */
	virtual void free(void *addr) = 0;
	virtual void free(void *addr, size_t size) override = 0;

	/**
	 * Return the sum of available memory
	 *
	 * Note that the returned value is not neccessarily allocatable
	 * because the memory may be fragmented.
	 */
	virtual size_t avail() const = 0;

	/**
	 * Check if address is inside an allocated block
	 *
	 * \param addr  address to check
	 *
	 * \return      true if address is inside an allocated block, false
	 *              otherwise
	 */
	virtual bool valid_addr(addr_t addr) const = 0;
};


void *operator new    (__SIZE_TYPE__, Genode::Allocator *);
void *operator new [] (__SIZE_TYPE__, Genode::Allocator *);

void *operator new    (__SIZE_TYPE__, Genode::Allocator &);
void *operator new [] (__SIZE_TYPE__, Genode::Allocator &);


/**
 * Delete operator invoked when an exception occurs during the construction of
 * a dynamically allocated object
 *
 * When an exception occurs during the construction of a dynamically allocated
 * object, the C++ standard devises the automatic invocation of the global
 * operator delete. When passing an allocator as argument to the new operator
 * (the typical case for Genode), the compiler magically calls the operator
 * delete taking the allocator type as second argument. This is how we end up
 * here.
 *
 * There is one problem though: We get the pointer of the to-be-deleted object
 * but not its size. But Genode's 'Allocator' interface requires the object
 * size to be passed as argument to 'Allocator::free()'.
 *
 * Even though in the general case, we cannot assume all 'Allocator'
 * implementations to remember the size of each allocated object, the commonly
 * used 'Heap', 'Sliced_heap', 'Allocator_avl', and 'Slab' do so and ignore the
 * size argument. When using either of those allocators, we are fine. Otherwise
 * we print a warning and pass the zero size argument anyway.
 *
 * :Warning: Never use an allocator that depends on the size argument of the
 *   'free()' method for the allocation of objects that may throw exceptions
 *   at their construction time!
 */
void operator delete (void *, Genode::Deallocator *);
void operator delete (void *, Genode::Deallocator &);


/**
 * The compiler looks for a delete operator signature matching the one of the
 * new operator. If none is found, it omits (silently!) the generation of
 * implicitly delete calls, which are needed when an exception is thrown within
 * the constructor of the object.
 */
inline void operator delete (void *ptr, Genode::Allocator *a) {
	operator delete (ptr, static_cast<Genode::Deallocator *>(a)); }

inline void operator delete (void *ptr, Genode::Allocator &a) {
	operator delete (ptr, *static_cast<Genode::Deallocator *>(&a)); }


/**
 * Destroy object
 *
 * For destroying an object, we need to specify the allocator that was used
 * by the object. Because we cannot pass the allocator directly to the
 * delete expression, we mimic the expression by using this template
 * function. The function explicitly calls the object destructor and
 * operator delete afterwards.
 *
 * For details see https://github.com/genodelabs/genode/issues/1030.
 *
 * \param T        implicit object type
 *
 * \param dealloc  reference or pointer to allocator from which the object
 *                 was allocated
 * \param obj      object to destroy
 */
template <typename T>
void Genode::destroy(auto && dealloc, T *obj)
{
	if (!obj)
		return;

	/* call destructors */
	obj->~T();

	/*
	 * Free memory at the allocator
	 *
	 * We have to use the delete operator instead of just calling
	 * 'dealloc.free' because the 'obj' pointer might not always point to the
	 * begin of the allocated block:
	 *
	 * If 'T' is the base class of another class 'A', 'obj' may refer
	 * to an instance of 'A'. In particular when 'A' used multiple inheritance
	 * with 'T' not being the first inhertited class, the pointer to the actual
	 * object differs from 'obj'.
	 *
	 * Given the pointer to the base class 'T', however, the delete operator is
	 * magically (by the means of the information found in the vtable of 'T')
	 * able to determine the actual pointer to the instance of 'A' and passes
	 * this pointer to 'free'.
	 */
	operator delete (obj, dealloc);
}

#endif /* _INCLUDE__BASE__ALLOCATOR_H_ */
