/**
 * \brief  Read-only memory modules
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-05-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ROM_FS_H_
#define _CORE__INCLUDE__ROM_FS_H_

#include <base/output.h>
#include <util/avl_string.h>

namespace Genode {
	struct Rom_module;
	struct Rom_fs;
}


struct Genode::Rom_module : Genode::Avl_string_base
{
	addr_t const addr = 0;
	size_t const size = 0;

	Rom_module() : Avl_string_base(nullptr) { }

	Rom_module(addr_t const addr, size_t const size, char const * const name)
	: Avl_string_base(name), addr(addr), size(size) { }

	bool valid() const { return size ? true : false; }

	void print(Genode::Output & out) const {
		Genode::print(out, Hex_range<addr_t>(addr, size), " ", name()); }
};


struct Genode::Rom_fs : Genode::Avl_tree<Genode::Avl_string_base>
{
	Rom_module const * find(char const * const name) const
	{
		return first() ? (Rom_module const *)first()->find_by_name(name)
		               : nullptr;
	}

	void print(Genode::Output & out) const
	{
		if (!first()) Genode::print(out, "No modules in Rom_fs\n");

		Genode::print(out, "ROM modules:\n");
		for_each([&] (Avl_string_base const & rom) {
			Genode::print(out, " ROM: ", *static_cast<Rom_module const *>(&rom), "\n"); });
	}
};

#endif /* _CORE__INCLUDE__ROM_FS_H_ */
