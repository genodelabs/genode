/*
 * \brief  Genode list with additional functions needed by NIC router
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIST_H_
#define _LIST_H_

/* Genode includes */
#include <util/list.h>
#include <base/allocator.h>

namespace Net { template <typename> class List; }


template <typename LT>
struct Net::List : Genode::List<LT>
{
	using Base = Genode::List<LT>;

	template <typename FUNC>
	void for_each(FUNC && functor)
	{
		for (LT *elem = Base::first(); elem; )
		{
			LT *const next = elem->Base::Element::next();
			functor(*elem);
			elem = next;
		}
	}

	void destroy_each(Genode::Deallocator &dealloc)
	{
		while (LT *elem = Base::first()) {
			Base::remove(elem);
			destroy(dealloc, elem);
		}
	}
};

#endif /* _LIST_H_ */
