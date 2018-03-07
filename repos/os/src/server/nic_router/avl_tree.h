/*
 * \brief  Genode AVL tree with additional functions needed by NIC router
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AVL_TREE_H_
#define _AVL_TREE_H_

/* Genode includes */
#include <util/avl_tree.h>
#include <base/allocator.h>

namespace Net { template <typename> class Avl_tree; }


template <typename ITEM_T>
struct Net::Avl_tree : Genode::Avl_tree<ITEM_T>
{
	using Base = Genode::Avl_tree<ITEM_T>;

	void destroy_each(Genode::Deallocator &dealloc)
	{
		while (ITEM_T *item = Base::first()) {
			Base::remove(item);
			destroy(dealloc, item);
		}
	}
};

#endif /* _AVL_TREE_H_*/
