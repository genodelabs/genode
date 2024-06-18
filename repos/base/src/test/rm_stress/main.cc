/*
 * \brief  Test program for stressing region-map attachments
 * \author Christian Helmuth
 * \date   2020-04-03
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/attached_ram_dataspace.h>


using namespace Genode;

struct X : Genode::Hex
{
	template <typename T>
	explicit X(T value) : Genode::Hex(value, OMIT_PREFIX, PAD) { }
};


struct Page : Attached_ram_dataspace
{
	unsigned char const color;

	Page(Env &env, unsigned char color)
	:
		Attached_ram_dataspace(env.ram(), env.rm(), 0x1000),
		color(color)
	{
		memset(local_addr<void>(), color, size());

		log("new page @ ", local_addr<void>(), " with color ", X(*local_addr<unsigned char>()));
	}
};


void Component::construct(Env &env)
{
	log("--- region-map attachment stress test ---");

	Page page[] = { { env, 0xaa }, { env, 0x55 } };

	enum { ROUNDS = 10000 };

	for (unsigned r = 0; r < ROUNDS; ++r) {
		for (unsigned i = 0; i < sizeof(page)/sizeof(*page); ++i) {
			addr_t const offset = 0;

			uint8_t volatile const *v =
				env.rm().attach(page[i].cap(), {
					.size       = page[i].size(),
					.offset     = offset,
					.use_at     = { },
					.at         = { },
					.executable = false,
					.writeable  = true
				}).convert<uint8_t *>(
					[&] (Region_map::Range range) { return (uint8_t *)range.start; },
					[&] (Region_map::Attach_error) { return nullptr; }
				);

			if (page[i].color != *v) {
				error("value @ ", v, "  ", X(*v), " != ", X(page[i].color), " in round ", r);
				env.parent().exit(-1);
			}

			env.rm().detach(addr_t(v));
		}
	}

	env.parent().exit(0);
}
