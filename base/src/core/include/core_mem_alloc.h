/*
 * \brief  Allocator infrastructure for core
 * \author Norman Feske
 * \author Stefan Kalkowski
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
#include <util.h>

namespace Genode {
	class Core_mem_allocator;
};


/**
 * Allocators for physical memory, core's virtual address space,
 * and core-local memory. The interface of this class is thread safe.
 * The class itself implements a ready-to-use memory allocator for
 * core that allows to allocate memory at page granularity only.
 */
class Genode::Core_mem_allocator : public Genode::Range_allocator
{
	public:

		using Page_allocator = Allocator_avl_tpl<Empty, get_page_size()>;
		using Phys_allocator = Synchronized_range_allocator<Page_allocator>;

	protected:

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

		bool _map_local(addr_t virt_addr, addr_t phys_addr, unsigned size);
		bool _unmap_local(addr_t virt_addr, unsigned size);

	public:

		/**
		 * Constructor
		 */
		Core_mem_allocator()
		: _phys_alloc(&_lock, this),
		  _virt_alloc(&_lock, this) { }

		/**
		 * Access physical-memory allocator
		 */
		Phys_allocator *phys_alloc() { return &_phys_alloc; }

		/**
		 * Access core's virtual-memory allocator
		 */
		Phys_allocator *virt_alloc() { return &_virt_alloc; }


		/*******************************
		 ** Range allocator interface **
		 *******************************/

		int          add_range(addr_t base, size_t size) { return -1; }
		int          remove_range(addr_t base, size_t size) { return -1; }
		Alloc_return alloc_aligned(size_t size, void **out_addr, int align = 0);
		Alloc_return alloc_addr(size_t size, addr_t addr) {
			return Alloc_return::RANGE_CONFLICT; }
		void         free(void *addr) {}
		size_t       avail() { return _phys_alloc.avail(); }

		bool valid_addr(addr_t addr) { return _virt_alloc.valid_addr(addr); }


		/*************************
		 ** Allocator interface **
		 *************************/

		bool alloc(size_t size, void **out_addr) {
			return alloc_aligned(size, out_addr).is_ok(); }

		void free(void *addr, size_t) { free(addr); }

		size_t consumed() { return _phys_alloc.consumed(); }
		size_t overhead(size_t size) { return _phys_alloc.overhead(size); }

		bool need_size_for_free() const override {
			return _phys_alloc.need_size_for_free(); }
};

#endif /* _CORE__INCLUDE__CORE_MEM_ALLOC_H_ */
