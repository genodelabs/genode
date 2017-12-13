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

namespace Net {
	template <typename> class List;
}

template <typename LT>
struct Net::List : Genode::List<LT>
{
	using Base = Genode::List<LT>;

	struct Empty : Genode::Exception { };

	template <typename FUNC>
	void for_each(FUNC && functor)
	{
		if (!Base::first()) {
			throw Empty();
		}
		for (LT * elem = Base::first(); elem;
		     elem = elem->Base::Element::next())
		{
			functor(*elem);
		}
	}
};

#endif /* _LIST_H_ */
