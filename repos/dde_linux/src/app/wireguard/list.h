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

	template <typename FUNC>
	void for_each(FUNC && functor) const
	{
		for (LT const *elem = Base::first(); elem; )
		{
			LT const *const next = elem->Base::Element::next();
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

	bool empty() const
	{
		return Base::first() == nullptr;
	}

	void insert_as_tail(LT const &le)
	{
		LT *elem { Base::first() };
		if (elem) {
			while (elem->Base::Element::next()) {
				elem = elem->Base::Element::next();
			}
		}
		Base::insert(&le, elem);
	}

	bool equal_to(List<LT> const &list) const
	{
		LT const *curr_elem_1 { Base::first() };
		LT const *curr_elem_2 { list.Base::first() };
		while (true) {

			if (curr_elem_1 == nullptr) {
				return curr_elem_2 == nullptr;
			}
			if (curr_elem_2 == nullptr) {
				return false;
			}
			LT const *const next_elem_1 {
				curr_elem_1->List<LT>::Element::next() };

			LT const *const next_elem_2 {
				curr_elem_2->List<LT>::Element::next() };

			if (!curr_elem_1->equal_to(*curr_elem_2)) {
				return false;
			}
			curr_elem_1 = next_elem_1;
			curr_elem_2 = next_elem_2;
		}
	}
};

#endif /* _LIST_H_ */
