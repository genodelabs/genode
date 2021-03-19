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

/* local includes */
#include <sorted_for_each.h>

namespace Genode {

	template <typename FN>
	static void for_each_subdir_name(Allocator &, Directory const &, FN const &);
}


template <typename FN>
static void Genode::for_each_subdir_name(Allocator &alloc, Directory const &dir,
                                         FN const &fn)
{
	using Dirname = Directory::Entry::Name;

	struct Name : Interface, private Dirname
	{
		using Dirname::string;

		Name(Dirname const &name) : Dirname(name) { }

		bool higher(Name const &other) const
		{
			return (strcmp(other.string(), string()) > 0);
		}
	};

	/* obtain list of sub directory names */
	Registry<Registered<Name>> names { };
	dir.for_each_entry([&] (Directory::Entry const &entry) {
		if (entry.dir())
			new (alloc) Registered<Name>(names, entry.name()); });

	auto destroy_names = [&] ()
	{
		names.for_each([&] (Registered<Name> &name) {
			destroy(alloc, &name); });
	};

	/* iterate over sorted list */
	try {
		sorted_for_each(alloc, names, [&] (Name const &name) {
			fn(name.string()); });
	}
	catch (...) {
		destroy_names();
		throw;
	}

	destroy_names();
}

#endif /* _FOR_EACH_SUBDIR_NAME_H_ */
