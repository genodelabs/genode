/*
 * \brief  Allocator AVL dump
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-10-15
 */

/*
 * Copyright (C) 2009-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>

void Genode::Allocator_avl_base::print(Genode::Output & out) const
{
	using Genode::print;
	unsigned long mem_size  = 0;
	unsigned long mem_avail = 0;

	print(out, "Allocator ", this, " dump:\n");

	_addr_tree.for_each([&] (Block const & b)
	{
		print(out, " Block: [", Hex(b.addr()), ",", Hex(b.addr() + b.size()),
		      "] ", "size=", Hex(b.size()), " avail=", Hex(b.avail()), " ",
		      "max_avail=", Hex(b.max_avail()), "\n");
		mem_size  += b.size();
		mem_avail += b.avail();
	});

	print(out, " => mem_size=", mem_size, " (", mem_size / 1024 / 1024 ,
	      " MB) / mem_avail=" , mem_avail , " (" , mem_avail / 1024 / 1024 ,
	      " MB)\n");
}
