/*
 * \brief  Expanding page table allocator
 * \author Johannes Schlatow
 * \date   2023-10-18
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__PC__EXPANDING_PAGE_TABLE_ALLOCATOR_H_
#define _SRC__DRIVERS__PLATFORM__PC__EXPANDING_PAGE_TABLE_ALLOCATOR_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_ram_dataspace.h>
#include <pd_session/pd_session.h>
#include <util/avl_tree.h>

namespace Driver {
	using namespace Genode;

	template <size_t TABLE_SIZE>
	class Expanding_page_table_allocator;
}


template <Genode::size_t TABLE_SIZE>
class Driver::Expanding_page_table_allocator
{
	public:

		struct Alloc_failed : Exception { };

	private:

		using Alloc_result  = Allocator::Alloc_result;
		using Alloc_error   = Allocator::Alloc_error;

		enum { MAX_CHUNK_SIZE = 2*1024*1024 };

		class Backing_store : Noncopyable
		{
			public:

				class Element : public Avl_node<Element>
				{
					private:

						Range_allocator        & _range_alloc;
						Attached_ram_dataspace   _dataspace;
						addr_t                   _virt_addr;
						addr_t                   _phys_addr;

						friend class Backing_store;

						Element * _matching_sub_tree(addr_t pa)
						{
							typename Avl_node<Element>::Side side = (pa > _phys_addr);
							return Avl_node<Element>::child(side);
						}

					public:

						Element(Range_allocator   & range_alloc,
						        Ram_allocator     & ram_alloc,
						        Region_map        & rm,
						        Pd_session        & pd,
						        size_t              size)
						: _range_alloc(range_alloc),
						  _dataspace(ram_alloc, rm, size, Genode::CACHED),
						  _virt_addr((addr_t)_dataspace.local_addr<void>()),
						  _phys_addr(pd.dma_addr(_dataspace.cap()))
						{
							_range_alloc.add_range(_phys_addr, size);
						}

						~Element() {
							_range_alloc.remove_range(_phys_addr, _dataspace.size()); }

						bool matches(addr_t pa)
						{
							return pa >= _phys_addr &&
							       pa < _phys_addr + _dataspace.size();
						}

						addr_t virt_addr(addr_t phys_addr) {
							return _virt_addr + (phys_addr - _phys_addr); }

						/*
						 * Avl_node interface
						 */
						bool higher(Element * other) {
							return other->_phys_addr > _phys_addr; }
				};

			private:

				Avl_tree<Element>  _tree { };
				Env              & _env;
				Allocator        & _md_alloc;
				Ram_allocator    & _ram_alloc;
				Range_allocator  & _range_alloc;
				size_t             _chunk_size;

			public:

				Backing_store(Env             & env,
				              Allocator       & md_alloc,
				              Ram_allocator   & ram_alloc,
				              Range_allocator & range_alloc,
				              size_t            start_size)
				: _env(env), _md_alloc(md_alloc), _ram_alloc(ram_alloc),
				  _range_alloc(range_alloc), _chunk_size(start_size)
				{ }

				~Backing_store();

				/* double backing store size (until MAX_CHUNK_SIZE is reached) */
				void grow();

				template <typename FN1, typename FN2>
				void with_virt_addr(addr_t pa, FN1 && match_fn, FN2 && no_match_fn)
				{
					Element * e = _tree.first();

					for (;;) {
						if (!e) break;

						if (e->matches(pa)) {
							match_fn(e->virt_addr(pa));
							return;
						}

						e = e->_matching_sub_tree(pa);
					}

					no_match_fn();
				};
		};

		Allocator_avl   _allocator;
		Backing_store   _backing_store;

		addr_t _alloc();

	public:

		Expanding_page_table_allocator(Genode::Env   & env,
		                               Allocator     & md_alloc,
		                               Ram_allocator & ram_alloc,
		                               size_t          start_count)
		: _allocator(&md_alloc),
		  _backing_store(env, md_alloc, ram_alloc, _allocator, start_count*TABLE_SIZE)
		{ }

		template <typename TABLE, typename FN1, typename FN2>
		void with_table(addr_t phys_addr, FN1 && match_fn, FN2 no_match_fn)
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");

			_backing_store.with_virt_addr(phys_addr,
				[&] (addr_t va) {
					match_fn(*(TABLE*)va); },
				no_match_fn);
		}

		template <typename TABLE> addr_t construct()
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");

			addr_t phys_addr = _alloc();
			_backing_store.with_virt_addr(phys_addr,
				[&] (addr_t va) {
					construct_at<TABLE>((void*)va); },
				[&] () {});

			return phys_addr;
		}

		template <typename TABLE> void destruct(addr_t phys_addr)
		{
			static_assert((sizeof(TABLE) == TABLE_SIZE), "unexpected size");

			with_table<TABLE>(phys_addr,
				[&] (TABLE & table) {
					table.~TABLE();
					_allocator.free((void*)phys_addr);
				},
				[&] () {
					error("Trying to destruct foreign table at ", Hex(phys_addr));
				});
		}
};


template <Genode::size_t TABLE_SIZE>
Driver::Expanding_page_table_allocator<TABLE_SIZE>::Backing_store::~Backing_store()
{
	while (_tree.first()) {
		Element * e = _tree.first();
		_tree.remove(e);
		destroy(_md_alloc, e);
	}
}


template <Genode::size_t TABLE_SIZE>
void Driver::Expanding_page_table_allocator<TABLE_SIZE>::Backing_store::grow()
{
	Element * e = new (_md_alloc) Element(_range_alloc, _ram_alloc,
	                                      _env.rm(), _env.pd(), _chunk_size);

	_tree.insert(e);

	/* double _chunk_size */
	_chunk_size = Genode::min(_chunk_size << 1, (size_t)MAX_CHUNK_SIZE);
}


template <Genode::size_t TABLE_SIZE>
Genode::addr_t Driver::Expanding_page_table_allocator<TABLE_SIZE>::_alloc()
{
	const unsigned align = (unsigned)Genode::log2(TABLE_SIZE);

	Alloc_result result = _allocator.alloc_aligned(TABLE_SIZE, align);

	if (result.failed()) {

		_backing_store.grow();

		/* retry allocation */
		result = _allocator.alloc_aligned(TABLE_SIZE, align);
	}

	return result.convert<addr_t>(
		[&] (void * ptr)  -> addr_t { return (addr_t)ptr; },
		[&] (Alloc_error) -> addr_t { throw Alloc_failed(); });
}

#endif /* _SRC__DRIVERS__PLATFORM__PC__EXPANDING_PAGE_TABLE_ALLOCATOR_H_ */
