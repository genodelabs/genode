/*
 * \brief  Allocator infrastructure for core
 * \author Norman Feske
 * \date   2009-10-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_MEM_ALLOC_H_
#define _CORE__INCLUDE__CORE_MEM_ALLOC_H_

#include <base/lock.h>
#include <base/sync_allocator.h>
#include <base/allocator_avl.h>

namespace Genode {

	/**
	 * Allocators for physical memory, core's virtual address space,
	 * and core-local memory. The interface of this class is thread safe.
	 */
	class Core_mem_allocator : public Allocator
	{
		public:

			typedef Synchronized_range_allocator<Allocator_avl> Phys_allocator;

		private:

			/**
			 * Unsynchronized allocator for core-mapped memory
			 *
			 * This is an allocator of core-mapped memory. It is meant to be used as
			 * meta-data allocator for the other allocators and as back end for core's
			 * synchronized memory allocator.
			 */
			class Mapped_mem_allocator : public Allocator
			{
				private:

					Allocator_avl     _alloc;
					Range_allocator *_phys_alloc;
					Range_allocator *_virt_alloc;

					/**
					 * Initial chunk to populate the core mem allocator
					 *
					 * This chunk is used at platform initialization time.
					 */
					char _initial_chunk[16*1024];

					/**
					 * Map physical page locally to specified virtual address
					 *
					 * \param virt_addr  core-local address
					 * \param phys_addr  physical memory address
					 * \param size_log2  size of memory block to map
					 * \return           true on success
					 */
					bool _map_local(addr_t virt_addr, addr_t phys_addr, unsigned size_log2);

				public:

					/**
					 * Constructor
					 *
					 * \param phys_alloc  allocator of physical memory
					 * \param virt_alloc  allocator of core-local virtual memory ranges
					 */
					Mapped_mem_allocator(Range_allocator *phys_alloc,
					                     Range_allocator *virt_alloc)
					: _alloc(0), _phys_alloc(phys_alloc), _virt_alloc(virt_alloc)
					{
						_alloc.add_range((addr_t)_initial_chunk, sizeof(_initial_chunk));
					}


					/*************************
					 ** Allocator interface **
					 *************************/

					bool alloc(size_t size, void **out_addr);
					void free(void *addr, size_t size) { _alloc.free(addr, size); }
					size_t consumed() { return _phys_alloc->consumed(); }
					size_t overhead(size_t size) { return _phys_alloc->overhead(size); }

					bool need_size_for_free() const override {
						return _phys_alloc->need_size_for_free(); }
			};


			/**
			 * Lock used for synchronization of all operations on the
			 * embedded allocators.
			 */
			Lock _lock;

			/**
			 * Synchronized allocator of physical memory ranges
			 *
			 * This allocator must only be used to allocate memory
			 * ranges at page granularity.
			 */
			Phys_allocator _phys_alloc;

			/**
			 * Synchronized allocator of core's virtual memory ranges
			 *
			 * This allocator must only be used to allocate memory
			 * ranges at page granularity.
			 */
			Phys_allocator _virt_alloc;

			/**
			 * Unsynchronized core-mapped memory allocator
			 *
			 * This allocator is internally used within this class for
			 * allocating meta data for the other allocators. It is not
			 * synchronized to avoid nested locking. The lock-guarded
			 * access to this allocator from the outer world is
			 * provided via the 'Allocator' interface implemented by
			 * 'Core_mem_allocator'. The allocator works at byte
			 * granularity.
			 */
			Mapped_mem_allocator _mem_alloc;

		public:

			/**
			 * Constructor
			 */
			Core_mem_allocator() :
				_phys_alloc(&_lock, &_mem_alloc),
				_virt_alloc(&_lock, &_mem_alloc),
				_mem_alloc(_phys_alloc.raw(), _virt_alloc.raw())
			{ }

			/**
			 * Access physical-memory allocator
			 */
			Phys_allocator *phys_alloc() { return &_phys_alloc; }

			/**
			 * Access core's virtual-memory allocator
			 */
			Phys_allocator *virt_alloc() { return &_virt_alloc; }


			/*************************
			 ** Allocator interface **
			 *************************/

			bool alloc(size_t size, void **out_addr)
			{
				Lock::Guard lock_guard(_lock);
				return _mem_alloc.alloc(size, out_addr);
			}

			void free(void *addr, size_t size)
			{
				Lock::Guard lock_guard(_lock);
				_mem_alloc.free(addr, size);
			}

			size_t consumed() { return _phys_alloc.consumed(); }
			size_t overhead(size_t size) { return _phys_alloc.overhead(size); }

			bool need_size_for_free() const override {
				return _phys_alloc.need_size_for_free(); }
	};
}

#endif /* _CORE__INCLUDE__CORE_MEM_ALLOC_H_ */
