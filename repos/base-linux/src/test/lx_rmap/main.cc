/*
 * \brief   Linux region-map test
 * \author  Christian Helmuth
 * \date    2013-09-06
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <base/crt0.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <util/misc_math.h>
#include <rm_session/connection.h>



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


int main()
{
	using namespace Genode;

	/* activate for early printf in Rm_session_mmap::attach() etc. */
	if (0) Thread_base::trace("FOO");

	/* induce initial heap expansion to remove RM noise */
	if (1) {
		void *addr(env()->heap()->alloc(0x100000));
		env()->heap()->free(addr, 0);
	}

	addr_t beg((addr_t)&blob_beg);
	addr_t end(align_addr((addr_t)&blob_end, 12));

	size_t size(end - beg);

	PLOG("blob region region [%016lx,%016lx) size=%zx", beg, end, size);

	/* RAM dataspace attachment overlapping binary */
	try {
		Ram_dataspace_capability ds(env()->ram_session()->alloc(size));

		PLOG("before RAM dataspace attach");
		env()->rm_session()->attach_at(ds, beg);
		PERR("after RAM dataspace attach -- ERROR");
	} catch (Rm_session::Region_conflict) {
		PLOG("OK caught Region_conflict exception");
	}

	/* empty managed dataspace overlapping binary */
	try {
		Rm_connection        rm(0, size);
		Dataspace_capability ds(rm.dataspace());

		PLOG("before sub-RM dataspace attach");
		env()->rm_session()->attach_at(ds, beg);
		PERR("after sub-RM dataspace attach -- ERROR");
	} catch (Rm_session::Region_conflict) {
		PLOG("OK caught Region_conflict exception");
	}

	/* sparsely populated managed dataspace in free VM area */
	try {
		Rm_connection rm(0, 0x100000);

		rm.attach_at(env()->ram_session()->alloc(0x1000), 0x1000);

		Dataspace_capability ds(rm.dataspace());

		PLOG("before populated sub-RM dataspace attach");
		char *addr = (char *)env()->rm_session()->attach(ds) + 0x1000;
		PLOG("after populated sub-RM dataspace attach / before touch");
		char const val = *addr;
		*addr = 0x55;
		PLOG("after touch (%x/%x)", val, *addr);
	} catch (Rm_session::Region_conflict) {
		PLOG("OK caught Region_conflict exception -- ERROR");
	}

	sleep_forever();
}
