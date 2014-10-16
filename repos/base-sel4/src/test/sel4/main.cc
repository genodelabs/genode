/*
 * \brief  Test for exercising the seL4 kernel interface
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/stdint.h>
#include <util/string.h>

/* seL4 includes */
#include <sel4/bootinfo.h>


static seL4_BootInfo const *boot_info()
{
	extern Genode::addr_t __initial_bx;
	return (seL4_BootInfo const *)__initial_bx;
}


static void dump_untyped_ranges(seL4_BootInfo const &bi,
                                unsigned start, unsigned size)
{
	for (unsigned i = start; i < start + size; i++) {

		/* index into 'untypedPaddrList' */
		unsigned const k = i - bi.untyped.start;

		PINF("                         [%02x] [%08lx,%08lx]",
		     i,
		     (long)bi.untypedPaddrList[k],
		     (long)bi.untypedPaddrList[k] + (1UL << bi.untypedSizeBitsList[k]) - 1);
	}
}


static void dump_boot_info(seL4_BootInfo const &bi)
{
	PINF("--- boot info at %p ---", &bi);
	PINF("ipcBuffer:               %p", bi.ipcBuffer);
	PINF("initThreadCNodeSizeBits: %d", (int)bi.initThreadCNodeSizeBits);
	PINF("untyped:                 [%x,%x)", bi.untyped.start, bi.untyped.end);

	dump_untyped_ranges(bi, bi.untyped.start,
	                    bi.untyped.end - bi.untyped.start);

	PINF("deviceUntyped:           [%x,%x)",
	     bi.deviceUntyped.start, bi.deviceUntyped.end);

	dump_untyped_ranges(bi, bi.deviceUntyped.start,
	                    bi.deviceUntyped.end - bi.deviceUntyped.start);
}


extern char _bss_start, _bss_end;

int main()
{
	seL4_BootInfo const *bi = boot_info();

	dump_boot_info(*bi);

	*(int *)0x1122 = 0;
	return 0;
}
