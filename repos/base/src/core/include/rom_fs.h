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

/* Genode includes */
#include <util/dictionary.h>

/* core includes */
#include <types.h>

namespace Core {

	using  Rom_name = String<64>;
	struct Rom_module;
	struct Rom_fs;
}


struct Core::Rom_module : Dictionary<Rom_module, Rom_name>::Element
{
	addr_t const addr = 0;
	size_t const size = 0;

	Rom_module(Dictionary<Rom_module, Rom_name> &dict, Rom_name const &name,
	           addr_t addr, size_t size)
	:
		Dictionary<Rom_module, Rom_name>::Element(dict, name),
		addr(addr), size(size)
	{ }

	bool valid() const { return size ? true : false; }

	void print(Output &out) const {
		Genode::print(out, Hex_range<addr_t>(addr, size), " ", name); }
};


struct Core::Rom_fs : Dictionary<Rom_module, Rom_name>
{
	void print(Output & out) const
	{
		Genode::print(out, "ROM modules:\n");
		for_each([&] (Rom_module const &rom) {
			Genode::print(out, " ROM: ", rom, "\n"); });
	}
};

#endif /* _CORE__INCLUDE__ROM_FS_H_ */
