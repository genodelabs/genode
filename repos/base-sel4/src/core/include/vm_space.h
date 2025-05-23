/*
 * \brief   Virtual-memory space
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__VM_SPACE_H_
#define _CORE__INCLUDE__VM_SPACE_H_

/* Genode includes */
#include <util/bit_allocator.h>
#include <base/thread.h>
#include <base/session_label.h>

/* core includes */
#include <base/internal/capability_space_sel4.h>
#include <base/internal/stack_area.h>
#include <page_table_registry.h>
#include <cnode.h>
#include <cap_sel_alloc.h>
#include <core_cspace.h>

namespace Core { class Vm_space; }


class Core::Vm_space
{
	private:

		Session_label _pd_label;

		Cap_sel_alloc &_cap_sel_alloc;

		Page_table_registry &_page_table_registry;

		unsigned const _id;
		Cap_sel  const _pd_sel;

		Range_allocator &_phys_alloc;

		enum {
			/**
			 * Number of entries of 3rd-level VM CNode ('_vm_3rd_cnode')
			 */
			VM_3RD_CNODE_SIZE_LOG2 = (CONFIG_WORD_SIZE == 32) ? 8 : 7,

			/**
			 * Number of entries of each leaf CNodes
			 */
			LEAF_CNODE_SIZE_LOG2 = (CONFIG_WORD_SIZE == 32) ? 8 : 7,
			LEAF_CNODE_SIZE      = 1UL << LEAF_CNODE_SIZE_LOG2,

			/**
			 * Number of leaf CNodes
			 */
			NUM_LEAF_CNODES_LOG2 = (CONFIG_WORD_SIZE == 32) ? 6 : 5,
			NUM_LEAF_CNODES      = 1UL << NUM_LEAF_CNODES_LOG2,

			NUM_CNODE_3RD_LOG2 = 3,
			NUM_CNODE_3RD      = 1UL << NUM_CNODE_3RD_LOG2,

			/**
			 * Maximum number of page tables and page frames for the VM space
			 */
			NUM_VM_SEL_LOG2 = LEAF_CNODE_SIZE_LOG2 + NUM_LEAF_CNODES_LOG2 +
			                  NUM_CNODE_3RD_LOG2,

			/**
			 * Number of remaining bits for vm_space to manage
			 */
			CNODE_BITS_2ND_3RD_4TH_LOG2 = 32 - Core_cspace::NUM_TOP_SEL_LOG2,

			/**
			 * Number of entries of the VM padding CNode ('_vm_pad_cnode')
			 */
			VM_2ND_CNODE_LOG2 = CNODE_BITS_2ND_3RD_4TH_LOG2
			                  - VM_3RD_CNODE_SIZE_LOG2
			                  - LEAF_CNODE_SIZE_LOG2,
		};

		Cnode &_top_level_cnode;
		Cnode &_phys_cnode;

		class Construct_cnode
		{
			private:

				/*
				 * We leave the CNode unconstructed at the time of its
				 * instantiation to be able to create an array of 'Construct_cnode'
				 * objects (where we cannot pass any arguments to the
				 * constructors of the individual objects).
				 */
				Constructible<Cnode>           _cnode     { };
				Cap_sel_alloc::Cap_sel_attempt _cnode_sel { Cap_sel_alloc::Cap_sel_error::DENIED };

			public:

				void construct(Cap_sel_alloc   &cap_sel_alloc,
				               Cap_sel          core_cnode_sel,
				               Range_allocator &phys_alloc,
				               auto const       size_log2)
				{
					_cnode_sel = cap_sel_alloc.alloc();

					_cnode_sel.with_result([&](auto result) {
						_cnode.construct(core_cnode_sel, Cnode_index(unsigned(result)),
						                 size_log2, phys_alloc);
					}, [](auto){ /* checked by constructed() */ });
				}

				void destruct(Cap_sel_alloc   &cap_sel_alloc,
				              Range_allocator &phys_alloc)
				{
					_cnode->destruct(phys_alloc);

					_cnode_sel.with_result([&](auto result) {
						cap_sel_alloc.free(Cap_sel(unsigned(result)));
					}, [](auto) { });

					_cnode_sel = { Cap_sel_alloc::Cap_sel_error::DENIED };
				}

				bool constructed() const
				{
					return !_cnode_sel.failed() && _cnode.constructed() &&
					        _cnode->constructed();
				}

				bool with_cnode(auto const &fn)
				{
					if (!constructed())
						return false;

					fn(*_cnode);

					return true;
				}
		};

		/**
		 * 2nd-level CNode for aligning '_4th' with the LSB of the CSpace
		 */
		Construct_cnode _vm_pad_cnode { };

		/***
		 * 4th-level CNode for storing page-table and page-frame capabilities
		 * 3rd-level CNode that hosts the leaf CNodes
		 */
		struct {
			Construct_cnode _4th [NUM_LEAF_CNODES];
			Construct_cnode _3rd { };
		} _cnodes [NUM_CNODE_3RD];

	public:

		/**
		 * Allocator for the selectors within '_cnodes'
		 */
		using Selector_allocator = Bit_allocator<1UL << NUM_VM_SEL_LOG2>;

		struct Map_attr
		{
			bool cached, write_combined, writeable, executable, flush_support;
		};

	private:

		Selector_allocator _sel_alloc { };

		/**
		 * Return leaf CNode that contains an index allocated from '_sel_alloc'
		 */
		bool _leaf_cnode(unsigned const idx, auto const &fn)
		{
			auto const l4 = (idx >> LEAF_CNODE_SIZE_LOG2) & (NUM_LEAF_CNODES - 1);
			auto const l3 = idx >> (LEAF_CNODE_SIZE_LOG2 + NUM_LEAF_CNODES_LOG2);

			ASSERT(l3 < NUM_CNODE_3RD);

			return _cnodes[l3]._4th[l4].with_cnode(fn);
		}

		/**
		 * Return entry within leaf cnode
		 */
		Cnode_index _leaf_cnode_entry(unsigned idx) const
		{
			return Cnode_index(idx & (LEAF_CNODE_SIZE - 1));
		}

		Mutex _mutex { };

		/**
		 * Return selector for a capability slot within '_cnodes'
		 */
		Cap_sel _idx_to_sel(addr_t const index) const
		{
			auto const leafs = LEAF_CNODE_SIZE_LOG2 + NUM_LEAF_CNODES_LOG2;
			auto const  low  = index & ((1ul << leafs) - 1);
			auto const  high = index >> leafs;
			auto const shift = VM_3RD_CNODE_SIZE_LOG2 + LEAF_CNODE_SIZE_LOG2;
			auto const   sel = (_id << CNODE_BITS_2ND_3RD_4TH_LOG2)
			                 | (high << shift) | low;

			return Cap_sel(unsigned(sel));
		}

		bool _flush(bool const flush_support, auto const &fn)
		{
			if (!flush_support) {
				warning("mapping cache full, but can't flush");
				return false;
			}

			warning("flush page table entries - mapping cache full - PD: ",
			        _pd_label.string());

			_page_table_registry.flush_pages(fn);

			return true;
		}

		bool _map_frame(addr_t const from_phys, addr_t const to_dest,
		                Map_attr const attr, bool guest, auto const &fn)
		{
			if (_page_table_registry.page_frame_at(to_dest)) {
				/*
				 * Valid behaviour if multiple threads concurrently
				 * causing the same page-fault. For the first thread the
				 * fault will be resolved, and the next thread will/would do
				 * it again. We just skip this attempt in order to avoid
				 * wasting of resources (idx selectors, creating kernel
				 * capabilities, causing kernel warning ...).
				 */
				return true;
			}

			/* allocate page-table-entry selector */
			auto pte_result = _sel_alloc.alloc();
			if (pte_result.failed()) {
				/* free all page-table-entry selectors and retry once */
				if (!_flush(attr.flush_support, fn))
					return false;

				pte_result = _sel_alloc.alloc();
				if (pte_result.failed())
					return false;
			}

			uint32_t pte_idx = 0;
			pte_result.with_result([&] (addr_t n) { pte_idx = unsigned(n); },
			                       [&] (auto) { /* handled before */ });

			/*
			 * Copy page-frame selector to pte_sel
			 *
			 * This is needed because each page-frame selector can be
			 * inserted into only a single page table.
			 */
			if (!_leaf_cnode(pte_idx, [&](auto &leaf_cnode) {
				leaf_cnode.copy(_phys_cnode,
				                Cnode_index((uint32_t)(from_phys >> get_page_size_log2())),
				                Cnode_index(_leaf_cnode_entry(pte_idx)));
			})) {
				_sel_alloc.free(pte_idx);
				return false;
			}

			/* remember relationship between pte_sel and the virtual address */
			try { _page_table_registry.insert_page_frame(to_dest, Cap_sel(pte_idx)); }
			catch (Page_table_registry::Mapping_cache_full) {

				/* free all entries of mapping cache and re-try once */
				if (!_flush(attr.flush_support, fn))
					return false;

				_page_table_registry.insert_page_frame(to_dest, Cap_sel(pte_idx));
			}

			/*
			 * Insert copy of page-frame selector into page table
			 */
			long ret = _map_page(Cap_sel(pte_idx), to_dest, attr, guest);
			if (ret != seL4_NoError) {
				error("seL4_*_Page_Map ", Hex(from_phys), "->",
				      Hex(to_dest), " returned ", ret);
				return false;
			}

			return true;
		}

		/**
		 * Platform specific map/unmap of a page frame
		 */
		long _map_page(Cap_sel const &idx, addr_t virt,
		               Map_attr attr, bool guest);
		long _unmap_page(Cap_sel const &idx);
		long _invalidate_page(Cap_sel const &, seL4_Word, seL4_Word);

		/**
		 * Allocate and install page structures for the protection domain.
		 */
		template <typename KOBJ>
		[[nodiscard]] bool _alloc_and_map(addr_t const virt,
		                                  long (&map_fn)(Cap_sel, Cap_sel, addr_t),
		                                  auto const &fn)
		{
			/* allocate page-* selector */
			auto const result_idx = _sel_alloc.alloc();

			if (result_idx.failed())
				return false;

			uint32_t pte_idx = 0;
			result_idx.with_result([&] (addr_t n) { pte_idx = unsigned(n); },
			                       [&] (auto) { /* handled before */ });

			auto phys_result = Untyped_memory::alloc_page(_phys_alloc);

			if (phys_result.failed()) {
				_sel_alloc.free(pte_idx);
				return false;
			}

			addr_t phys = 0;

			try {
				phys_result.with_result([&](auto &result) {
					bool ok = _leaf_cnode(pte_idx, [&](auto &leaf_cnode) {
						phys = addr_t(result.ptr);
						seL4_Untyped const service = Untyped_memory::untyped_sel(phys).value();

						create<KOBJ>(service, leaf_cnode.sel(),
						             _leaf_cnode_entry(pte_idx));
					});

					result.deallocate = !ok;
				}, [&] (auto) { /* handled before by explicit failed() check */ });
			} catch (...) {
				_sel_alloc.free(pte_idx);
				return false;
			}

			if (!phys) {
				_sel_alloc.free(pte_idx);
				return false;
			}

			Cap_sel const pt_sel = _idx_to_sel(pte_idx);

			long const result = map_fn(pt_sel, _pd_sel, virt);
			if (result != seL4_NoError) {
				error("seL4_*_Page*_Map(,", Hex(virt), ") returned ",
				      result);

				error("leaking idx, untyped memory and phys addr in _alloc_and_map");
				return false;
			}

			if (fn(Cap_sel(pte_idx), phys))
				return true;

			_unmap_and_free(Cap_sel(pte_idx), phys);

			return false;
		}

		void _unmap_and_free(Cap_sel const idx, addr_t const paddr)
		{
			_leaf_cnode(idx.value(), [&](auto &leaf_cnode) {
				leaf_cnode.remove(idx);

				_sel_alloc.free(idx.value());

				Untyped_memory::free_page(_phys_alloc, paddr);
			});
		}

		void _for_each_l3_cnodes(auto const &fn)
		{
			for (unsigned l3 = 0; l3 < NUM_CNODE_3RD; l3++)
				fn(l3, _cnodes[l3]._3rd, _cnodes[l3]._4th);
		}

		void _revert_cnodes_l3(auto const &fn)
		{
			for (int l3 = NUM_CNODE_3RD - 1; l3 >= 0; l3--)
				fn(l3, _cnodes[l3]._3rd, _cnodes[l3]._4th);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param pd_sel              selector for page directory
		 * \param cap_sel_alloc       capability-selector allocator used for
		 *                            VM CNodes
		 * \param phys_alloc          backing store for the CNodes
		 * \param top_level_cnode     top-level CNode to insert 'vm_pad_cnode_sel'
		 * \param id                  ID used as index in 'top_level_cnode'
		 * \param page_table_registry association of VM CNode selectors with
		 *                            with virtual addresses
		 */
		Vm_space(Cap_sel              pd_sel,
		         Cap_sel_alloc       &cap_sel_alloc,
		         Range_allocator     &phys_alloc,
		         Cnode               &top_level_cnode,
		         Cnode               &core_cnode,
		         Cnode               &phys_cnode,
		         unsigned             id,
		         Page_table_registry &page_table_registry,
		         const char *         label)
		:
			_pd_label(label),
			_cap_sel_alloc(cap_sel_alloc),
			_page_table_registry(page_table_registry),
			_id(id), _pd_sel(pd_sel),
			_phys_alloc(phys_alloc),
			_top_level_cnode(top_level_cnode),
			_phys_cnode(phys_cnode)
		{
			static_assert(NUM_CNODE_3RD_LOG2 <= VM_2ND_CNODE_LOG2);

			_vm_pad_cnode.construct(_cap_sel_alloc, core_cnode.sel(),
			                        phys_alloc, VM_2ND_CNODE_LOG2);

			_vm_pad_cnode.with_cnode([&](auto &vm_pad_cnode) {
				Cnode_base const cspace(Cap_sel(seL4_CapInitThreadCNode), 32);

				/* insert 2nd-level VM-pad CNode into 1st-level CNode */
				_top_level_cnode.copy(cspace, vm_pad_cnode.sel(), Cnode_index(_id));

				_for_each_l3_cnodes([&](int l3, auto &cnode_3rd, auto &leafs) {

					cnode_3rd.construct(_cap_sel_alloc, core_cnode.sel(),
						                phys_alloc, VM_3RD_CNODE_SIZE_LOG2);

					for (unsigned l4 = 0; l4 < NUM_LEAF_CNODES; l4++) {

						/* init leaf VM CNode */
						leafs[l4].construct(_cap_sel_alloc, core_cnode.sel(),
						                    phys_alloc, LEAF_CNODE_SIZE_LOG2);

						/* insert leaf VM CNode into 3nd-level VM CNode */
						cnode_3rd.with_cnode([&](auto &cnode_3rd) {
							leafs[l4].with_cnode([&](auto &cnode_4th) {
								cnode_3rd.copy(cspace, cnode_4th.sel(), Cnode_index(l4));
							});
						});
					}

					/* insert 3rd-level VM CNode into 2nd-level VM-pad CNode */
					cnode_3rd.with_cnode([&](auto &cnode) {
						vm_pad_cnode.copy(cspace, cnode.sel(), Cnode_index(l3));
					});
				});
			});
		}

		bool constructed()
		{
			if (!_vm_pad_cnode.constructed())
				return false;

			bool success = true;

			_for_each_l3_cnodes([&](int, auto &cnode_3rd, auto &leafs) {
				if (!success)
					return;

				success = cnode_3rd.constructed();

				if (!success)
					return;

				for (unsigned l4 = 0; l4 < NUM_LEAF_CNODES; l4++) {
					success = leafs[l4].constructed();

					if (!success)
						break;
				}
			});

			return success;
		}

		~Vm_space()
		{
			/* delete copy of the mapping's page-frame selectors */
			_page_table_registry.flush_all([&] (Cap_sel const &idx, addr_t const virt) {

				long err = _unmap_page(idx);
				if (err != seL4_NoError)
					error("unmap ", Hex(virt), " failed, ", idx, " res=", err);

				_leaf_cnode(idx.value(), [&](auto &leaf_cnode) {
					leaf_cnode.remove(_leaf_cnode_entry(idx.value()));
				});

				_sel_alloc.free(idx.value());

				return true;
			}, [&] (Cap_sel const &idx, addr_t const paddr) {
				_unmap_and_free(idx, paddr);
			});

			_vm_pad_cnode.with_cnode([&](auto &vm_pad_cnode) {
				_revert_cnodes_l3([&](auto l3, auto &cnode_3rd, auto &leafs) {
					for (int l4 = NUM_LEAF_CNODES - 1; l4 >= 0; l4--) {
						cnode_3rd.with_cnode([&](auto & cnode) {
							cnode.remove(Cnode_index(l4));
							leafs[l4].destruct(_cap_sel_alloc, _phys_alloc);
						});
					}

					vm_pad_cnode.remove(Cnode_index(l3));

					cnode_3rd.with_cnode([&](auto &cnode) {
						cnode.destruct(_phys_alloc);
					});
					cnode_3rd.destruct(_cap_sel_alloc, _phys_alloc);
				});

				_top_level_cnode.remove(Cnode_index(_id));

				vm_pad_cnode.destruct(_phys_alloc);
				_cap_sel_alloc.free(vm_pad_cnode.sel());
			});
		}

		bool map(addr_t const from_phys, addr_t const to_virt,
		         size_t const num_pages, Map_attr const attr)
		{
			auto fn_unmap = [&] (Cap_sel const &idx, addr_t const v_addr)
			{
				/* XXX - INITIAL_IPC_BUFFER can't be re-mapped currently */
				if (v_addr == 0x1000)
					return false;
				/* XXX - UTCB can't be re-mapped currently */
				if (stack_area_virtual_base() <= v_addr
				    && (v_addr < stack_area_virtual_base() +
				                 stack_area_virtual_size())
				    && !((v_addr + 0x1000) & (stack_virtual_size() - 1)))
						return false;

				long err = _unmap_page(idx);
				if (err != seL4_NoError)
					error("unmap failed, idx=", idx, " res=", err);

				_leaf_cnode(idx.value(), [&](auto &leaf_cnode) {
					leaf_cnode.remove(_leaf_cnode_entry(idx.value()));
				});

				_sel_alloc.free(idx.value());

				return true;
			};

			Mutex::Guard guard(_mutex);

			bool ok = true;

			for (size_t i = 0; i < num_pages; i++) {
				addr_t const offset = i << get_page_size_log2();

				if (_map_frame(from_phys + offset, to_virt + offset, attr,
				               false /* host page table */, fn_unmap))
					continue;

				ok = false;

				warning("mapping failed ", Hex(from_phys + offset),
				        " -> ", Hex(to_virt + offset), " ",
				        !attr.flush_support ? "core" : "");
			}

			return ok;
		}

		void map_guest(addr_t const from_phys, addr_t const guest_phys,
		               size_t const num_pages, Map_attr const attr)
		{
			auto fn_unmap = [&] (Cap_sel const &idx, addr_t const) {
				long err = _unmap_page(idx);
				if (err != seL4_NoError)
					error("unmap failed, idx=", idx, " res=", err);

				_leaf_cnode(idx.value(), [&](auto &leaf_cnode) {
					leaf_cnode.remove(_leaf_cnode_entry(idx.value()));
				});

				_sel_alloc.free(idx.value());

				return true;
			};

			Mutex::Guard guard(_mutex);

			for (size_t i = 0; i < num_pages; i++) {
				addr_t const offset = i << get_page_size_log2();

				_map_frame(from_phys + offset, guest_phys + offset, attr,
				           true /* guest page table */, fn_unmap);
			}
		}

		bool unmap(addr_t const virt, size_t const num_pages,
		           bool const invalidate = false)
		{
			bool unmap_success = true;

			Mutex::Guard guard(_mutex);

			for (size_t i = 0; unmap_success && i < num_pages; i++) {
				addr_t const offset = i << get_page_size_log2();

				_page_table_registry.flush_page(virt + offset, [&] (Cap_sel const &idx, addr_t) {

					if (invalidate) {
						long result = _invalidate_page(idx, virt + offset,
						                               virt + offset + 4096);
						if (result != seL4_NoError)
							error("invalidating ", Hex(virt + offset),
							      " failed, idx=", idx, " result=", result);
					}

					long result = _unmap_page(idx);
					if (result != seL4_NoError) {
						error("unmap ", Hex(virt + offset), " failed, idx=",
						      idx, " result=", result);
						unmap_success = false;
					}

					_leaf_cnode(idx.value(), [&](auto &leaf_cnode) {
						leaf_cnode.remove(_leaf_cnode_entry(idx.value()));
					});

					_sel_alloc.free(idx.value());
				});
			}
			return unmap_success;
		}

		[[nodiscard]] bool unsynchronized_alloc_page_tables(addr_t, addr_t);
		[[nodiscard]] bool unsynchronized_alloc_guest_page_tables(addr_t, addr_t);

		[[nodiscard]] bool alloc_page_tables(addr_t const start, addr_t const size)
		{
			Mutex::Guard guard(_mutex);
			return unsynchronized_alloc_page_tables(start, size);
		}

		[[nodiscard]] bool alloc_guest_page_tables(addr_t const start, addr_t const size)
		{
			Mutex::Guard guard(_mutex);
			return unsynchronized_alloc_guest_page_tables(start, size);
		}

		Session_label const & pd_label() const { return _pd_label; }

		auto max_page_frames() { return 1ul << NUM_VM_SEL_LOG2; }
};

#endif /* _CORE__INCLUDE__VM_SPACE_H_ */
