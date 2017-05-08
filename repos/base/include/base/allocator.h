/*
 * \brief  Generic allocator interface
 * \author Norman Feske
 * \date   2006-04-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ALLOCATOR_H_
#define _INCLUDE__BASE__ALLOCATOR_H_

#include <base/stdint.h>
#include <base/exception.h>
#include <base/quota_guard.h>

namespace Genode {

	struct Deallocator;
	struct Allocator;
	struct Range_allocator;

	template <typename T, typename DEALLOC> void destroy(DEALLOC && dealloc, T *obj);
}


/**
 * Deallocator interface
 */
struct Genode::Deallocator
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


struct Genode::Allocator : Deallocator
{
	/**
	 * Exception type
	 */
	typedef Out_of_ram Out_of_memory;

	/**
	 * Destructor
	 */
	virtual ~Allocator() { }

	/**
	 * Allocate block
	 *
	 * \param size      block size to allocate
	 * \param out_addr  resulting pointer to the new block,
	 *                  undefined in the error case
	 *
	 * \throw           Out_of_ram
	 * \throw           Out_of_caps
	 *
	 * \return          true on success
	 */
	virtual bool alloc(size_t size, void **out_addr) = 0;

	/**
	 * Allocate typed block
	 *
	 * This template allocates a typed block returned as a pointer to
	 * a non-void type. By providing this method, we prevent the
	 * compiler from warning us about "dereferencing type-punned
	 * pointer will break strict-aliasing rules".
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	template <typename T> bool alloc(size_t size, T **out_addr)
	{
		void *addr = 0;
		bool ret = alloc(size, &addr);
		*out_addr = (T *)addr;
		return ret;
	}

	/**
	 * Return total amount of backing store consumed by the allocator
	 */
	virtual size_t consumed() const { return 0; }

	/**
	 * Return meta-data overhead per block
	 */
	virtual size_t overhead(size_t size) const = 0;

	/**
	 * Allocate block and signal error as an exception
	 *
	 * \param size  block size to allocate
	 *
	 * \throw       Out_of_ram
	 * \throw       Out_of_caps
	 *
	 * \return      pointer to the new block
	 */
	void *alloc(size_t size)
	{
		void *result = 0;
		if (!alloc(size, &result))
			throw Out_of_memory();

		return result;
	}
};


struct Genode::Range_allocator : Allocator
{
	/**
	 * Destructor
	 */
	virtual ~Range_allocator() { }

	/**
	 * Add free address range to allocator
	 */
	virtual int add_range(addr_t base, size_t size) = 0;

	/**
	 * Remove address range from allocator
	 */
	virtual int remove_range(addr_t base, size_t size) = 0;

	/**
	 * Return value of allocation functons
	 *
	 * 'OK'              on success, or
	 * 'OUT_OF_METADATA' if meta-data allocation failed, or
	 * 'RANGE_CONFLICT'  if no fitting address range is found
	 */
	struct Alloc_return
	{
		enum Value { OK = 0, OUT_OF_METADATA = -1, RANGE_CONFLICT = -2 };
		Value const value;
		Alloc_return(Value value) : value(value) { }

		bool ok()    const { return value == OK; }
		bool error() const { return !ok(); }

		/*
		 * \deprecated  use 'ok' and 'error' instead
		 */
		bool is_ok()    const { return ok(); }
		bool is_error() const { return error(); }
	};

	/**
	 * Allocate block
	 *
	 * \param size      size of new block
	 * \param out_addr  start address of new block,
	 *                  undefined in the error case
	 * \param align     alignment of new block specified
	 *                  as the power of two
	 */
	virtual Alloc_return alloc_aligned(size_t size, void **out_addr, int align, addr_t from=0, addr_t to = ~0UL) = 0;

	/**
	 * Allocate block at address
	 *
	 * \param size   size of new block
	 * \param addr   desired address of block
	 *
	 * \return  'ALLOC_OK' on success, or
	 *          'OUT_OF_METADATA' if meta-data allocation failed, or
	 *          'RANGE_CONFLICT' if specified range is occupied
	 */
	virtual Alloc_return alloc_addr(size_t size, addr_t addr) = 0;

	/**
	 * Free a previously allocated block
	 *
	 * NOTE: We have to declare the 'Allocator::free(void *)' method
	 * here as well to make the compiler happy. Otherwise the C++
	 * overload resolution would not find 'Allocator::free(void *)'.
	 */
	virtual void free(void *addr) = 0;
	virtual void free(void *addr, size_t size) = 0;

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
template <typename T, typename DEALLOC>
void Genode::destroy(DEALLOC && dealloc, T *obj)
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
