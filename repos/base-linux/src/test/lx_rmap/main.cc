/*
 * \brief   Linux region-map test
 * \author  Christian Helmuth
 * \date    2013-09-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <base/thread.h>
#include <util/misc_math.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

using namespace Genode;

static void blob() __attribute__((used));
static void blob()
{
	asm volatile (
		".balign 4096, -1\n"
		"blob_beg:\n"
		".space 16*4096, -2\n"
		"blob_end:\n"
		".global blob_beg\n"
		".global blob_end\n"
		: : : );
}

extern unsigned long blob_beg;
extern unsigned long blob_end;

struct Main
{
	Heap heap;

	Main(Env &env);
};

Main::Main(Env &env) : heap(env.ram(), env.rm())
{
	/* activate for early printf in Rm_session_mmap::attach() etc. */
	if (0) Thread::trace("FOO");

	/* induce initial heap expansion to remove RM noise */
	if (1) {
		void *addr;
		heap.alloc(0x100000, &addr);
		heap.free(addr, 0);
	}

	addr_t beg((addr_t)&blob_beg);
	addr_t end(align_addr((addr_t)&blob_end, 12));

	size_t size(end - beg);

	log("blob region region ", Hex_range<addr_t>(beg, size), " size=", size);

	/* RAM dataspace attachment overlapping binary */
	try {
		Ram_dataspace_capability ds(env.ram().alloc(size));

		log("before RAM dataspace attach");
		env.rm().attach_at(ds, beg);
		error("after RAM dataspace attach -- ERROR");
		env.parent().exit(-1);
	} catch (Region_map::Region_conflict) {
		log("OK caught Region_conflict exception");
	}

	/* empty managed dataspace overlapping binary */
	try {
		Rm_connection        rm_connection(env);
		Region_map_client    rm(rm_connection.create(size));
		Dataspace_capability ds(rm.dataspace());

		log("before sub-RM dataspace attach");
		env.rm().attach_at(ds, beg);
		error("after sub-RM dataspace attach -- ERROR");
		env.parent().exit(-1);
	} catch (Region_map::Region_conflict) {
		log("OK caught Region_conflict exception");
	}

	/* sparsely populated managed dataspace in free VM area */
	try {
		Rm_connection rm_connection(env);
		Region_map_client rm(rm_connection.create(0x100000));

		rm.attach_at(env.ram().alloc(0x1000), 0x1000);

		Dataspace_capability ds(rm.dataspace());

		log("before populated sub-RM dataspace attach");
		char *addr = (char *)env.rm().attach(ds) + 0x1000;
		log("after populated sub-RM dataspace attach / before touch");
		char const val = *addr;
		*addr = 0x55;
		log("after touch (", val, "/", *addr, ")");
	} catch (Region_map::Region_conflict) {
		error("Caught Region_conflict exception -- ERROR");
		env.parent().exit(-1);
	}
	env.parent().exit(0);
}

void Component::construct(Env &env) { static Main main(env); }
