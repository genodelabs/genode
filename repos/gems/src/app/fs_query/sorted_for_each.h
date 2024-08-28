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
#include <util/dictionary.h>

namespace Genode {
	template <typename T, typename FN>
	static inline void sorted_for_each(Allocator &, Registry<T> const &, FN const &);
}


/**
 * Execute 'fn' for each registry element
 *
 * The type T must be equipped with a method name() and a type Name:
 *
 *   const & Name name() const
 *
 * The allocator passed as 'alloc' is used to for temporary allocations.
 */
template <typename T, typename FN>
static inline void Genode::sorted_for_each(Allocator         &alloc,
                                           Registry<T> const &registry,
                                           FN          const &fn)
{
	using Name = T::Name;
	struct SortedItem : Dictionary<SortedItem, Name>::Element
	{
		T const &element;

		SortedItem(Dictionary<SortedItem, Name> & dict, Name const & name, T const & element)
		: Dictionary<SortedItem, Name>::Element(dict, name),
		  element(element)
		{ }
	};

	/* build temporary Dictionary of sorted and unique elements */
	using Dict = Dictionary<SortedItem, Name>;
	Dict sorted { };
	registry.for_each([&] (T const &element) {
		/* skip duplicates */
		if (sorted.exists(element.name())) return;

		new (alloc) SortedItem(sorted, element.name(), element);
	});

	auto destroy_element = [&] (SortedItem &item) {
		destroy(alloc, &item); };

	/* iterate over sorted elements, 'fn' may throw */
	try {
		sorted.for_each([&] (SortedItem const &item) {
			fn(item.element); });
	}
	catch (...) {
		while (sorted.with_any_element(destroy_element)) { }
		throw;
	}

	while (sorted.with_any_element(destroy_element)) { }
}

#endif /* _SORTED_FOR_EACH_H_ */
