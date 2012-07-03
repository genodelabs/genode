/*
 * \brief  Lock-guarded allocator interface
 * \author Norman Feske
 * \date   2008-08-05
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SYNC_ALLOCATOR_H_
#define _INCLUDE__BASE__SYNC_ALLOCATOR_H_

#include <base/allocator.h>
#include <base/lock.h>

namespace Genode {

	/**
	 * Lock-guarded allocator
	 *
	 * This class wraps the complete 'Allocator' interface while
	 * preventing concurrent calls to the wrapped allocator implementation.
	 *
	 * \param ALLOCATOR_IMPL  class implementing the 'Allocator'
	 *                        interface
	 */
	template <typename ALLOCATOR_IMPL>
	class Synchronized_allocator : public Allocator
	{
		private:

			Lock            _default_lock;
			Lock           *_lock;
			ALLOCATOR_IMPL _alloc;

		public:

			/**
			 * Constructor
			 *
			 * This constructor uses an embedded lock for synchronizing the
			 * access to the allocator.
			 */
			Synchronized_allocator()
			: _lock(&_default_lock) { }

			/**
			 * Constructor
			 *
			 * This constructor uses an embedded lock for synchronizing the
			 * access to the allocator.
			 */
			explicit Synchronized_allocator(Allocator *metadata_alloc)
			: _lock(&_default_lock), _alloc(metadata_alloc) { }

			/**
			 * Return reference to wrapped (non-thread-safe) allocator
			 *
			 * This is needed, for example, if the wrapped allocator implements
			 * methods in addition to the Range_allocator interface.
			 */
			ALLOCATOR_IMPL *raw() { return &_alloc; }

			/*************************
			 ** Allocator interface **
			 *************************/

			bool alloc(size_t size, void **out_addr)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.alloc(size, out_addr);
			}

			void free(void *addr, size_t size)
			{
				Lock::Guard lock_guard(*_lock);
				_alloc.free(addr, size);
			}

			size_t consumed()
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.consumed();
			}

			size_t overhead(size_t size)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.overhead(size);
			}
	};

	/**
	 * Lock-guarded range allocator
	 *
	 * This class wraps the complete 'Range_allocator' interface while
	 * preventing concurrent calls to the wrapped allocator implementation.
	 *
	 * \param ALLOCATOR_IMPL  class implementing the 'Range_allocator'
	 *                        interface
	 */
	template <typename ALLOCATOR_IMPL>
	class Synchronized_range_allocator : public Range_allocator
	{
		private:

			Lock           _default_lock;
			Lock          *_lock;
			ALLOCATOR_IMPL _alloc;

		public:

			/**
			 * Constructor
			 *
			 * This constructor uses an embedded lock for synchronizing the
			 * access to the allocator.
			 */
			Synchronized_range_allocator()
			: _lock(&_default_lock) { }

			/**
			 * Constructor
			 *
			 * This constructor uses an embedded lock for synchronizing the
			 * access to the allocator.
			 */
			explicit Synchronized_range_allocator(Allocator *metadata_alloc)
			: _lock(&_default_lock), _alloc(metadata_alloc) { }

			/**
			 * Constructor
			 *
			 * \param lock  use specified lock rather then an embedded lock for
			 *              synchronization
			 *
			 * This constructor is useful if multiple allocators must be
			 * synchronized with each other. In such as case, the shared
			 * lock can be passed to each 'Synchronized_range_allocator'
			 * instance.
			 */
			Synchronized_range_allocator(Lock *lock, Allocator *metadata_alloc)
			: _lock(lock), _alloc(metadata_alloc) { }

			/**
			 * Return reference to wrapped (non-thread-safe) allocator
			 *
			 * This is needed, for example, if the wrapped allocator implements
			 * methods in addition to the Range_allocator interface.
			 *
			 * NOTE: Synchronize accesses to the raw allocator by facilitating
			 * the lock() member function.
			 */
			ALLOCATOR_IMPL *raw() { return &_alloc; }

			/**
			 * Return reference to synchronization lock
			 */
			Lock *lock() { return _lock; }


			/*************************
			 ** Allocator interface **
			 *************************/

			bool alloc(size_t size, void **out_addr)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.alloc(size, out_addr);
			}

			void free(void *addr, size_t size)
			{
				Lock::Guard lock_guard(*_lock);
				_alloc.free(addr, size);
			}

			size_t consumed()
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.consumed();
			}

			size_t overhead(size_t size)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.overhead(size);
			}


			/*******************************
			 ** Range-allocator interface **
			 *******************************/

			int add_range(addr_t base, size_t size)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.add_range(base, size);
			}

			int remove_range(addr_t base, size_t size)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.remove_range(base, size);
			}

			bool alloc_aligned(size_t size, void **out_addr, int align = 0)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.alloc_aligned(size, out_addr, align);
			}

			Alloc_return alloc_addr(size_t size, addr_t addr)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.alloc_addr(size, addr);
			}

			void free(void *addr)
			{
				Lock::Guard lock_guard(*_lock);
				_alloc.free(addr);
			}

			size_t avail()
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.avail();
			}

			bool valid_addr(addr_t addr)
			{
				Lock::Guard lock_guard(*_lock);
				return _alloc.valid_addr(addr);
			}
	};
}

#endif /* _INCLUDE__BASE__SYNC_ALLOCATOR_H_ */
