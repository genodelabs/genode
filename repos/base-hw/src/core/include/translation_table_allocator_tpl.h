/*
 * \brief  Translation table allocator template implementation
 * \author Stefan Kalkowski
 * \date   2015-06-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRANSLATION_TABLE_ALLOCATOR_TPL_H_
#define _CORE__INCLUDE__TRANSLATION_TABLE_ALLOCATOR_TPL_H_

#include <base/stdint.h>
#include <util/bit_allocator.h>
#include <core_mem_alloc.h>
#include <translation_table.h>
#include <translation_table_allocator.h>

namespace Genode {

	/**
	 * Statically dimensioned translation table allocator
	 *
	 * /param TABLES  count of tables the allocator provides as maximum
	 */
	template <unsigned TABLES> class Translation_table_allocator_tpl;
}


template <unsigned TABLES>
class Genode::Translation_table_allocator_tpl
{
	private:

		/**
		 * The actual allocator interface cannot be implmented by our
		 * template class itself, because of its strict alignment constraints
		 * and therefore the impossibility to have a vtable pointer at the
		 * beginning of the object's layout. Therefore, we use a private
		 * implementation of the interface and aggregate it.
		 */
		class Allocator;

		struct Table {
			enum { SIZE = 1 << Translation_table::TABLE_LEVEL_X_SIZE_LOG2 };

			uint8_t data[SIZE];
		};

		Table     _tables[TABLES];
		Allocator _alloc;

	public:

		static constexpr unsigned ALIGN = Table::SIZE;

		Translation_table_allocator_tpl() : _alloc(_tables, (addr_t)&_tables) {}
		Translation_table_allocator_tpl(Core_mem_allocator *cma)
		: _alloc(_tables, (addr_t)cma->phys_addr((void*)&_tables)) {}

		Translation_table_allocator * alloc() { return &_alloc; }

		static Translation_table_allocator_tpl *
		base(Translation_table_allocator * alloc)
		{
			return (Translation_table_allocator_tpl*)((addr_t)alloc
			                                          - sizeof(Table)*TABLES);
		}
} __attribute__((aligned(ALIGN)));


template <unsigned TABLES>
class Genode::Translation_table_allocator_tpl<TABLES>::Allocator
: public Translation_table_allocator
{
	private:

		Table                 *_tables;
		Bit_allocator<TABLES> _free_tables;
		addr_t                _phys_addr;

		/**
		 * Allocate a page
		 *
		 * \returns pointer to new slab, or nullptr if allocation failed
		 */
		void *_alloc()
		{
			try {
				return &_tables[_free_tables.alloc()];
			} catch(typename Bit_allocator<TABLES>::Out_of_indices&) {}
			return nullptr;
		}

		/**
		 * Free a given page
		 *
		 * \param addr  virtual address of page to free
		 */
		void _free(void *addr)
		{
			_free_tables.free(((addr_t)addr - (addr_t)_tables)
			                 / sizeof(Table));
		}

	public:

		Allocator(Table * tables, addr_t phys_addr)
		: _tables(tables), _phys_addr(phys_addr) {}

		void * phys_addr(void * addr)
		{
			return (void*)((addr_t)addr - (addr_t)_tables
			               + _phys_addr);
		}

		void * virt_addr(void * addr)
		{
			return (void*)((addr_t)_tables + ((addr_t)addr
			               - _phys_addr));
		}


		/************************
		 * Allocator interface **
		 ************************/

		bool   alloc(size_t, void **addr) override {
			return (*addr = _alloc()); }
		void   free(void *addr, size_t)   override { _free(addr); }
		size_t consumed()           const override { return 0; }
		size_t overhead(size_t)     const override { return 0; }
		bool   need_size_for_free() const override { return false; }
};

#endif /* _CORE__INCLUDE__TRANSLATION_TABLE_ALLOCATOR_TPL_H_ */
