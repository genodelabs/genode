/*
 * \brief   Utilities for manipulating seL4 CNodes
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CNODE_H_
#define _CORE__INCLUDE__CNODE_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <base/exception.h>
#include <base/allocator.h>

/* core includes */
#include <kernel_object.h>

namespace Genode {

	class Cnode_base;
	class Cnode;
}


class Genode::Cnode_base
{
	private:

		Cap_sel const _sel;
		size_t  const _size_log2;

	public:

		typedef Cnode_index Index;

		Cap_sel sel()       const { return _sel; }
		size_t  size_log2() const { return _size_log2; }

		/**
		 * Copy selector from another CNode
		 */
		void copy(Cnode_base const &from, Index from_idx, Index to_idx)
		{
			seL4_CNode     const service    = sel().value();
			seL4_Word      const dest_index = to_idx.value();
			uint8_t        const dest_depth = size_log2();
			seL4_CNode     const src_root   = from.sel().value();
			seL4_Word      const src_index  = from_idx.value();
			uint8_t        const src_depth  = from.size_log2();
			seL4_CapRights const rights     = seL4_AllRights;

			int const ret = seL4_CNode_Copy(service, dest_index, dest_depth,
			                                src_root, src_index, src_depth, rights);
			if (ret != 0) {
				warning(__FUNCTION__, ": seL4_CNode_Copy (",
				        (void*)from_idx.value(), ") returned ", ret);
			}
		}

		void copy(Cnode_base const &from, Index idx) { copy(from, idx, idx); }

		/**
		 * Mint selector from another CNode
		 */
		void mint(Cnode_base const &from, Index from_idx, Index to_idx)
		{
			seL4_CNode     const service    = sel().value();
			seL4_Word      const dest_index = to_idx.value();
			uint8_t        const dest_depth = size_log2();
			seL4_CNode     const src_root   = from.sel().value();
			seL4_Word      const src_index  = from_idx.value();
			uint8_t        const src_depth  = from.size_log2();
			seL4_CapRights const rights     = seL4_AllRights;
			seL4_CapData_t const badge      = seL4_CapData_Badge_new(to_idx.value());

			int const ret = seL4_CNode_Mint(service, dest_index, dest_depth,
			                                src_root, src_index, src_depth,
			                                rights, badge);
			ASSERT(ret == seL4_NoError);
		}

		/**
		 * Delete selector from CNode
		 */
		void remove(Index idx)
		{
			int ret = seL4_CNode_Delete(sel().value(), idx.value(), size_log2());
			if (ret != seL4_NoError)
				error(__PRETTY_FUNCTION__, ": seL4_CNode_Delete (",
				      Hex(idx.value()), ") returned ", ret);
		}

		/**
		 * Move selector from another CNode
		 */
		void move(Cnode_base const &from, Index from_idx, Index to_idx)
		{
			seL4_CNode const service    = sel().value();
			seL4_Word  const dest_index = to_idx.value();
			uint8_t    const dest_depth = size_log2();
			seL4_CNode const src_root   = from.sel().value();
			seL4_Word  const src_index  = from_idx.value();
			uint8_t    const src_depth  = from.size_log2();

			int const ret = seL4_CNode_Move(service, dest_index, dest_depth,
			                                src_root, src_index, src_depth);
			if (ret != 0) {
				warning(__FUNCTION__, ": seL4_CNode_Move (",
				        (void*)from_idx.value(), ") returned ", ret);
			}
		}

		void move(Cnode_base const &from, Index idx) { move(from, idx, idx); }

		/**
		 * Constructor
		 */
		Cnode_base(Cap_sel sel, size_t size_log2)
		: _sel(sel), _size_log2(size_log2) { }
};


class Genode::Cnode : public Cnode_base, Noncopyable
{
	private:

		addr_t _phys = 0UL;

	public:

		class Untyped_lookup_failed : Exception { };
		class Retype_untyped_failed : Exception { };

		/**
		 * Constructor
		 *
		 * \param parent_sel  CNode where to place the cap selector of the
		 *                    new CNode
		 * \param dst_idx     designated index within 'parent_sel' referring to
		 *                    the created CNode
		 * \param size_log2   number of entries in CNode
		 * \param phys_alloc  physical-memory allocator used for allocating
		 *                    the CNode backing store
		 *
		 * \throw Phys_alloc_failed
		 * \throw Untyped_address::Lookup_failed
		 *
		 * \deprecated
		 */
		Cnode(Cap_sel parent_sel, Index dst_idx, size_t size_log2,
		      Range_allocator &phys_alloc)
		:
			Cnode_base(dst_idx, size_log2)
		{
			_phys = create<Cnode_kobj>(phys_alloc, parent_sel, dst_idx, size_log2);
		}

		/**
		 * Constructor
		 *
		 * \param parent_sel    CNode where to place the cap selector of the
		 *                      new CNode
		 * \param dst_idx       designated index within 'parent_sel' referring to
		 *                      the created CNode
		 * \param size_log2     number of entries in CNode
		 * \param untyped_pool  initial untyped memory pool used for allocating
		 *                      the CNode backing store
		 *
		 * \throw Retype_untyped_failed
		 * \throw Initial_untyped_pool::Initial_untyped_pool_exhausted
		 */
		Cnode(Cap_sel parent_sel, Index dst_idx, size_t size_log2,
		      Initial_untyped_pool &untyped_pool)
		:
			Cnode_base(dst_idx, size_log2)
		{
			create<Cnode_kobj>(untyped_pool, parent_sel, dst_idx, size_log2);
		}

		void destruct(Range_allocator &phys_alloc, bool revoke = false)
		{
			/* revert phys allocation */

			if (!_phys) {
				error("invalid call to destruct Cnode");
				return;
			}

			if (revoke) {
				int ret = seL4_CNode_Revoke(seL4_CapInitThreadCNode,
				                            sel().value(), 32);
				if (ret)
					error(__PRETTY_FUNCTION__, ": seL4_CNode_Revoke (",
					      Hex(sel().value()), ") returned ", ret);
			}

			int ret = seL4_CNode_Delete(seL4_CapInitThreadCNode,
			                            sel().value(), 32);
			if (ret != seL4_NoError)
				error(__PRETTY_FUNCTION__, ": seL4_CNode_Delete (",
				      Hex(sel().value()), ") returned ", ret);

			Untyped_memory::free_page(phys_alloc, _phys);

			_phys = ~0UL;
		}

		~Cnode()
		{
			if (_phys == ~0UL)
				return;

			/* convert CNode back to untyped memory */

			error(__FUNCTION__, " - not implemented phys=", Hex(_phys),
			      " sel=", Hex(sel().value()));
		}
};

#endif /* _CORE__INCLUDE__CNODE_H_ */
