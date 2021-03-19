/*
 * \brief  Utility for accessing registry elements in a sorted order
 * \author Norman Feske
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SORTED_FOR_EACH_H_
#define _SORTED_FOR_EACH_H_

#include <base/registry.h>
#include <base/allocator.h>

namespace Genode {
	template <typename T, typename FN>
	static inline void sorted_for_each(Allocator &, Registry<T> const &, FN const &);
}


/**
 * Execute 'fn' for each registry element
 *
 * The type T must be equipped with a method that defines the sort criterion:
 *
 *   bool higher(T const &other) const
 *
 * It must implement a strict order over all registry elements. E.g., if the
 * registry contains a set of names, no name must occur twice. The allocator
 * passed as 'alloc' is used to for temporary allocations.
 */
template <typename T, typename FN>
static inline void Genode::sorted_for_each(Allocator         &alloc,
                                           Registry<T> const &registry,
                                           FN          const &fn)
{
	struct Sorted_item : Avl_node<Sorted_item>
	{
		T const &element;

		Sorted_item(T const &element) : element(element) { }

		bool higher(Sorted_item const *item) const
		{
			return item ? element.higher(item->element) : false;
		}
	};

	/* build temporary AVL tree of sorted elements */
	Avl_tree<Sorted_item> sorted { };
	registry.for_each([&] (T const &element) {
		sorted.insert(new (alloc) Sorted_item(element)); });

	auto destroy_sorted = [&] ()
	{
		while (Sorted_item *item = sorted.first()) {
			sorted.remove(item);
			destroy(alloc, item);
		}
	};

	/* iterate over sorted elements, 'fn' may throw */
	try {
		sorted.for_each([&] (Sorted_item const &item) {
			fn(item.element); });
	}
	catch (...) {
		destroy_sorted();
		throw;
	}

	destroy_sorted();
}

#endif /* _SORTED_FOR_EACH_H_ */
