/*
 * \brief  Test for downgrading RAM dataspaces to read-only access
 * \author Norman Feske
 * \date   2026-03-03
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_ram_dataspace.h>


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	Attached_ram_dataspace ram { env.ram(), env.rm(), 4096 };

	(void)ram.bytes().as_output([&] (Output &out) { print(out, "original content"); });
	log("RAM contains: ", Cstring(ram.bytes().start, ram.bytes().num_bytes));

	log("sealing");
	env.pd().seal_ram(ram.cap());
	log("RAM contains: ", Cstring(ram.bytes().start, ram.bytes().num_bytes));

	Attached_dataspace ro { env.rm(), ram.cap() };
	log("read-only mapping contains: ", Cstring(ro.bytes().start, ro.bytes().num_bytes));

	log("modify original mapping");
	(void)ram.bytes().as_output([&] (Output &out) { print(out, "modified content"); });
	log("read-only mapping contains: ", Cstring(ro.bytes().start, ro.bytes().num_bytes));

	log("try to modify read-only mapping");
	(void)ro.bytes().as_output([&] (Output &out) { print(out, "illegal access"); });

	/* the illegal access should have triggered an unresolvable page fault */
	log("this should never happen");
}
