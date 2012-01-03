/*
 * \brief  Generic allocator interface
 * \author Norman Feske
 * \date   2006-04-16
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__ALLOCATOR_H_
#define _INCLUDE__BASE__ALLOCATOR_H_

#include <base/stdint.h>
#include <base/exception.h>

namespace Genode {

	class Allocator
	{
		public:

			/*********************
			 ** Exception types **
			 *********************/

			class Out_of_memory : public Exception { };

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
			 * \return          true on success
			 */
			virtual bool alloc(size_t size, void **out_addr) = 0;

			/**
			 * Allocate typed block
			 *
			 * This template allocates a typed block returned as a pointer to
			 * a non-void type. By providing this function, we prevent the
			 * compiler from warning us about "dereferencing type-punned
			 * pointer will break strict-aliasing rules".
			 */
			template <typename T> bool alloc(size_t size, T **out_addr)
			{
				void *addr = 0;
				bool ret = alloc(size, &addr);
				*out_addr = (T *)addr;
				return ret;
			}

			/**
			 * Free block a previously allocated block
			 */
			virtual void free(void *addr, size_t size) = 0;

			/**
			 * Return total amount of backing store consumed by the allocator
			 */
			virtual size_t consumed() { return 0; }

			/**
			 * Return meta-data overhead per block
			 */
			virtual size_t overhead(size_t size) = 0;


			/***************************
			 ** Convenience functions **
			 ***************************/

			/**
			 * Allocate block and signal error as an exception
			 *
			 * \param   size block size to allocate
			 * \return  pointer to the new block
			 * \throw   Out_of_memory
			 */
			void *alloc(size_t size)
			{
				void *result = 0;
				if (!alloc(size, &result))
					throw Out_of_memory();

				return result;
			}
	};


	class Range_allocator : public Allocator
	{
		public:

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
			 * Allocate block
			 *
			 * \param size      size of new block
			 * \param out_addr  start address of new block,
			 *                  undefined in the error case
			 * \param align     alignment of new block specified
			 *                  as the power of two
			 * \return          true on success
			 */
			virtual bool alloc_aligned(size_t size, void **out_addr, int align = 0) = 0;

			enum Alloc_return { ALLOC_OK = 0, OUT_OF_METADATA = -1, RANGE_CONFLICT = -2 };

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
			 * NOTE: We have to declare the 'Allocator::free' function here
			 * as well to make gcc happy.
			 */
			virtual void free(void *addr) = 0;
			virtual void free(void *addr, size_t size) = 0;

			/**
			 * Return the sum of available memory
			 *
			 * Note that the returned value is not neccessarily allocatable
			 * because the memory may be fragmented.
			 */
			virtual size_t avail() = 0;

			/**
			 * Check if address is inside an allocated block
			 *
			 * \param addr  address to check
			 *
			 * \return      true if address is inside an allocated block, false
			 *              otherwise
			 */
			virtual bool valid_addr(addr_t addr) = 0;
	};


	/**
	 * Destroy object
	 *
	 * For destroying an object, we need to specify the allocator
	 * that was used by the object. Because we cannot pass the
	 * allocator directly to the delete operator, we mimic the
	 * delete operator using this template function.
	 *
	 * \param T      implicit object type
	 *
	 * \param alloc  allocator from which the object was allocated
	 * \param obj    object to destroy
	 */
	template <typename T>
	void destroy(Allocator *alloc, T *obj)
	{
		if (!obj)
			return;

		/* call destructors */
		obj->~T();

		/* free memory at the allocator */
		alloc->free(obj, sizeof(T));
	}
}

void *operator new    (Genode::size_t size, Genode::Allocator *allocator);
void *operator new [] (Genode::size_t size, Genode::Allocator *allocator);

#endif /* _INCLUDE__BASE__ALLOCATOR_H_ */
