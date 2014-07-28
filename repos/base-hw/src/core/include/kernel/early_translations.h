/**
 * \brief   Tools for early translation tables
 * \author  Stefan Kalkowski
 * \author  Martin Stein
 * \date    2014-08-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__EARLY_TRANSLATIONS_H_
#define _KERNEL__EARLY_TRANSLATIONS_H_

/* core includes */
#include <page_slab.h>
#include <translation_table.h>

namespace Genode
{
	/**
	 * Dummy back-end allocator for early translation tables
	 */
	class Early_translations_allocator;

	/**
	 * Aligned slab for early translation tables
	 */
	class Early_translations_slab;
}

namespace Kernel
{
	using Genode::Early_translations_allocator;
	using Genode::Early_translations_slab;
}

class Genode::Early_translations_allocator : public Genode::Core_mem_translator
{
	public:

		Early_translations_allocator() { }
		int add_range(addr_t base, size_t size) { return -1; }
		int remove_range(addr_t base, size_t size) { return -1; }
		Alloc_return alloc_aligned(size_t size, void **out_addr, int align) {
			return Alloc_return::RANGE_CONFLICT; }
		Alloc_return alloc_addr(size_t size, addr_t addr) {
			return Alloc_return::RANGE_CONFLICT; }
		void   free(void *addr) {}
		size_t avail() { return 0; }
		bool valid_addr(addr_t addr) { return false; }
		bool alloc(size_t size, void **out_addr) { return false; }
		void free(void *addr, size_t) {  }
		size_t overhead(size_t size) { return 0; }
		bool need_size_for_free() const override { return false; }
		void * phys_addr(void * addr) { return addr; }
		void * virt_addr(void * addr) { return addr; }
};

class Genode::Early_translations_slab : public Genode::Page_slab
{
	public:

		typedef Genode::Core_mem_translator Allocator;

		enum {
			ALIGN_LOG2 = Genode::Translation_table::ALIGNM_LOG2,
			ALIGN      = 1 << ALIGN_LOG2,
		};

		/**
		 * Constructor
		 */
		Early_translations_slab(Allocator * const alloc) : Page_slab(alloc) {
			assert(Genode::aligned(this, ALIGN_LOG2)); }

} __attribute__((aligned(Early_translations_slab::ALIGN)));

#endif /* _KERNEL__EARLY_TRANSLATIONS_H_ */
