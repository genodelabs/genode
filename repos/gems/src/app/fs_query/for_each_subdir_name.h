/*
 * \brief  Utility for iterating over subdirectory names
 * \author Norman Feske
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FOR_EACH_SUBDIR_NAME_H_
#define _FOR_EACH_SUBDIR_NAME_H_

/* Genode includes */
#include <os/vfs.h>
#include <util/dictionary.h>

namespace Genode {

	template <typename FN>
	static void for_each_subdir_name(Allocator &, Directory const &, FN const &);
}


template <typename FN>
static void Genode::for_each_subdir_name(Allocator &alloc, Directory const &dir,
                                         FN const &fn)
{
	using Name = Directory::Entry::Name;

	struct Dirname : Dictionary<Dirname, Name>::Element
	{
		Dirname(Dictionary<Dirname, Name> & dict, Name const & name)
		: Dictionary<Dirname, Name>::Element(dict, name)
		{ }
	};

	/* obtain dictionary of sub directory names */
	Dictionary<Dirname, Name> names { };
	dir.for_each_entry([&] (Directory::Entry const &entry) {
		if (entry.dir())
			new (alloc) Dirname(names, entry.name()); });

	auto destroy_element = [&] (Dirname &element) {
		destroy(alloc, &element); };

	/* iterate over dictionary */
	try {
		names.for_each([&] (Dirname const &element) {
			fn(element.name.string()); });
	}
	catch (...) {
		while (names.with_any_element(destroy_element)) { }
		throw;
	}

	while (names.with_any_element(destroy_element)) { }
}

#endif /* _FOR_EACH_SUBDIR_NAME_H_ */
