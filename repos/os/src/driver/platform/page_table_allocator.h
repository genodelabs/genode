/*
 * \brief  Expanding page table allocator
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \date   2023-10-18
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__PAGE_TABLE_ALLOCATOR_H_
#define _SRC__DRIVERS__PLATFORM__PAGE_TABLE_ALLOCATOR_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_ram_dataspace.h>
#include <cpu/page_table.h>
#include <pd_session/pd_session.h>
#include <util/avl_tree.h>

namespace Driver {
	using namespace Genode;

	class Page_table_allocator;
}


class Driver::Page_table_allocator
{
	public:

		static constexpr size_t TABLE_SIZE = 1 << SIZE_LOG2_4KB;

		struct Alloc_failed : Exception { };

	private:

		using Alloc_result  = Allocator::Alloc_result;
		using Alloc_error   = Genode::Alloc_error;

		enum { MAX_CHUNK_SIZE = 2*1024*1024 };

		class Backing_store : Noncopyable
		{
			public:

				class Element : public Avl_node<Element>
				{
					private:

						Range_allocator         &_range_alloc;
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

						Element(Range_allocator   &range_alloc,
						        Ram_allocator     &ram_alloc,
						        Env::Local_rm     &rm,
						        Pd_session        &pd,
						        size_t             size)
						: _range_alloc(range_alloc),
						  _dataspace(ram_alloc, rm, size, Genode::CACHED),
						  _virt_addr((addr_t)_dataspace.local_addr<void>()),
						  _phys_addr(pd.dma_addr(_dataspace.cap()))
						{
							(void)_range_alloc.add_range(_phys_addr, size);
						}

						~Element() {
							(void)_range_alloc.remove_range(_phys_addr, _dataspace.size()); }

						bool matches(addr_t pa) const
						{
							return pa >= _phys_addr &&
							       pa < _phys_addr + _dataspace.size();
						}

						addr_t virt_addr(addr_t phys_addr) const {
							return _virt_addr + (phys_addr - _phys_addr); }

						void phys_addr(addr_t virt_addr, auto const &fn) const
						{
							if (virt_addr >= _virt_addr &&
							    virt_addr <  _virt_addr + _dataspace.size())
								fn(_phys_addr + (virt_addr - _virt_addr));
						}

						/*
						 * Avl_node interface
						 */
						bool higher(Element * other) const {
							return other->_phys_addr > _phys_addr; }
				};

			private:

				friend class Page_table_allocator;

				Avl_tree<Element> _tree { };
				Env              &_env;
				Allocator        &_md_alloc;
				Ram_allocator    &_ram_alloc;
				Range_allocator  &_range_alloc;
				size_t            _chunk_size;

			public:

				Backing_store(Env             &env,
				              Allocator       &md_alloc,
				              Ram_allocator   &ram_alloc,
				              Range_allocator &range_alloc,
				              size_t           start_size)
				: _env(env), _md_alloc(md_alloc), _ram_alloc(ram_alloc),
				  _range_alloc(range_alloc), _chunk_size(start_size)
				{ }

				~Backing_store();

				/* double backing store size (until MAX_CHUNK_SIZE is reached) */
				void grow();

				template <typename FN1, typename FN2>
				void with_virt_addr(addr_t pa, FN1 && match_fn, FN2 && no_match_fn) const
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

		Page_table_allocator(Genode::Env   &env,
		                     Allocator     &md_alloc,
		                     Ram_allocator &ram_alloc,
		                     size_t         start_count)
		: _allocator(&md_alloc),
		  _backing_store(env, md_alloc, ram_alloc, _allocator, start_count*TABLE_SIZE)
		{ }

		template <typename TABLE, typename FN1, typename FN2>
		void with_table(addr_t phys_addr, FN1 && match_fn, FN2 no_match_fn) const
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
				[&] (TABLE &table) {
					table.~TABLE();
					_allocator.free((void*)phys_addr);
				},
				[&] () {
					error("Trying to destruct foreign table at ", Hex(phys_addr));
				});
		}

		using Error  = Page_table_error;
		using Result = Attempt<Ok, Error>;

		template <typename TABLE, typename ENTRY>
		Result create(typename ENTRY::access_t &descriptor)
		{
			Result result = Error::DENIED;

			/* Unfortunately _alloc() still throws exceptions */
			try {
				addr_t phys_addr = _alloc();
				_backing_store.with_virt_addr(phys_addr,
					[&] (addr_t va) {
						construct_at<TABLE>((void*)va);
						descriptor = ENTRY::create(phys_addr);
						result = Ok();
					},
					[&] () {});
			} catch(Out_of_ram)  {
				result = Error::OUT_OF_RAM;
			} catch(Out_of_caps) {
				result = Error::OUT_OF_CAPS;
			} catch(...) {
				error("error during table creation!"); }

			return result;
		}

		template <typename TABLE>
		void destroy(TABLE &table)
		{
			class Error {};
			using Result = Attempt<addr_t, Error>;
			Result result = Error();

			/*
			 * Non-optimal: we've to iterate through the whole tree,
			 *              there is no indexing with virtual addresses
			 */
			_backing_store._tree.for_each([&] (auto const &e) {
				e.phys_addr((addr_t)&table, [&] (addr_t pa) {
					result = pa; });
			});

			result.with_result(
				[&] (addr_t phys_addr) {
					table.~TABLE();
					_allocator.free((void*)phys_addr);
				}, [] (auto) {});
		}

		template <typename TABLE>
		Result lookup(addr_t phys_addr, auto const fn)
		{
			Result result = Error::INVALID_RANGE;
			with_table<TABLE>(phys_addr,
				[&] (TABLE &t) { result = fn(t); },
				[] () {});
			return result;
		}
};

#endif /* _SRC__DRIVERS__PLATFORM__PAGE_TABLE_ALLOCATOR_H_ */
