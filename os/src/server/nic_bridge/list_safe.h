/*
 * \brief  Thread-safe list implementation
 * \author Stefan Kalkowski
 * \date   2010-08-23
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIST_SAFE_H_
#define _LIST_SAFE_H_

#include <base/lock_guard.h>
#include <util/list.h>

/**
 * Lock-guarded avl-tree implementation.
 */
template <typename LE>
class List_safe : public Genode::List<LE>
{
	private:

		Genode::Lock _lock;

	public:

		void insert(LE *item)
		{
			Genode::Lock::Guard lock_guard(_lock);
			Genode::List<LE>::insert(item);
		}

		void remove(LE *item)
		{
			Genode::Lock::Guard lock_guard(_lock);
			Genode::List<LE>::remove(item);
		}
};

#endif /* _LIST_SAFE_H_ */
