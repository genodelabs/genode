/*
 * \brief  Allocator dump helpers
 * \author Norman Feske
 * \date   2009-10-15
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/allocator_avl.h>

using namespace Genode;


void Allocator_avl_base::Block::dump()
{
	log(" Block: [", Hex(addr()), ",", Hex(addr() + size()), "] ",
	    "size=", Hex(size()), " avail=", Hex(avail()), " ",
		"max_avail=", Hex(max_avail()));
}


void Allocator_avl_base::dump_addr_tree(Block *addr_node)
{
	bool top = false;
	static unsigned long mem_size;
	static unsigned long mem_avail;

	if (addr_node == 0) {
		addr_node = _addr_tree.first();

		log("Allocator ", this, " dump:");
		mem_size = mem_avail = 0;
		top = true;
	}

	if (!addr_node) return;

	if (addr_node->child(0))
		dump_addr_tree(addr_node->child(0));

	Block *b = (Block *)addr_node;
	b->dump();
	mem_size  += b->size();
	mem_avail += b->avail();

	if (addr_node->child(1))
		dump_addr_tree(addr_node->child(1));

	if (top)
		log(" => mem_size=", mem_size, " (", mem_size / 1024 / 1024, " MB) ",
		    "/ mem_avail=", mem_avail, " (", mem_avail / 1024 / 1024, " MB)");
}
