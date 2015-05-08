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

		unsigned const _sel;
		size_t   const _size_log2;

	public:

		unsigned sel()       const { return _sel; }
		size_t   size_log2() const { return _size_log2; }

		/**
		 * Copy selector from another CNode
		 */
		void copy(Cnode_base const &from, unsigned from_idx, unsigned to_idx)
		{
			seL4_CNode     const service    = sel();
			seL4_Word      const dest_index = to_idx;
			uint8_t        const dest_depth = size_log2();
			seL4_CNode     const src_root   = from.sel();
			seL4_Word      const src_index  = from_idx;
			uint8_t        const src_depth  = from.size_log2();
			seL4_CapRights const rights     = seL4_AllRights;

			int const ret = seL4_CNode_Copy(service, dest_index, dest_depth,
			                                src_root, src_index, src_depth, rights);
			if (ret != 0) {
				PWRN("%s: seL4_CNode_Copy (0x%x) returned %d", __FUNCTION__,
				     from_idx, ret);
			}
		}

		void copy(Cnode_base const &from, unsigned idx) { copy(from, idx, idx); }

		/**
		 * Delete selector from CNode
		 */
		void remove(unsigned idx)
		{
			seL4_CNode_Delete(sel(), idx, size_log2());
		}

		/**
		 * Move selector from another CNode
		 */
		void move(Cnode_base const &from, unsigned from_idx, unsigned to_idx)
		{
			seL4_CNode const service    = sel();
			seL4_Word  const dest_index = to_idx;
			uint8_t    const dest_depth = size_log2();
			seL4_CNode const src_root   = from.sel();
			seL4_Word  const src_index  = from_idx;
			uint8_t    const src_depth  = from.size_log2();

			int const ret = seL4_CNode_Move(service, dest_index, dest_depth,
			                                src_root, src_index, src_depth);
			if (ret != 0) {
				PWRN("%s: seL4_CNode_Move (0x%x) returned %d", __FUNCTION__,
				     from_idx, ret);
			}
		}

		void move(Cnode_base const &from, unsigned idx) { move(from, idx, idx); }

		/**
		 * Constructor
		 */
		Cnode_base(unsigned sel, size_t size_log2)
		: _sel(sel), _size_log2(size_log2) { }
};


class Genode::Cnode : public Cnode_base, Noncopyable
{
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
		 */
		Cnode(unsigned parent_sel, unsigned dst_idx, size_t size_log2,
		      Range_allocator &phys_alloc)
		:
			Cnode_base(dst_idx, size_log2)
		{
			Kernel_object::create<Kernel_object::Cnode>(phys_alloc, parent_sel,
			                                            dst_idx, size_log2);
		}

		~Cnode()
		{
			/* convert CNode back to untyped memory */

			/* revert phys allocation */

			PDBG("not implemented");
		}
};

#endif /* _CORE__INCLUDE__CNODE_H_ */
