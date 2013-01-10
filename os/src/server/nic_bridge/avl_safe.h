/*
 * \brief  Thread-safe Avl_tree implementation
 * \author Stefan Kalkowski
 * \date   2010-08-20
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _AVL_SAFE_H_
#define _AVL_SAFE_H_

/* Genode */
#include <base/lock.h>
#include <base/lock_guard.h>
#include <util/avl_tree.h>

/**
 * Lock-guarded avl-tree implementation.
 */
template <typename NT>
class Avl_tree_safe : public Genode::Avl_tree<NT>
{
	private:

		Genode::Lock _lock;

	public:

		void insert(Genode::Avl_node<NT> *node)
		{
			Genode::Lock::Guard lock_guard(_lock);
			Genode::Avl_tree<NT>::insert(node);
		}

		void remove(Genode::Avl_node<NT> *node)
		{
			Genode::Lock::Guard lock_guard(_lock);
			Genode::Avl_tree<NT>::remove(node);
		}
};

#endif /* _AVL_SAFE_H_ */
