/*
 * \brief  GRUB multi-boot information handling
 * \author Christian Helmuth
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <multiboot.h>
#include <util/misc_math.h>
#include <pistachio/kip.h>

/* core includes */
#include <util.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/bootinfo.h>
#include <l4/sigma0.h>
}

using namespace Genode;


static const bool verbose = false;


#define VPRINTF(fmt...) if (verbose) printf(fmt); else {}


void Multiboot_info::print_debug()
{
	printf("TODO Multiboot_info does not support print_debug.");
}


unsigned Multiboot_info::num_modules()
{
	using namespace Pistachio;

	unsigned int i = 0;
	L4_Word_t entries;
	L4_BootRec_t *rec;
	for (entries = L4_BootInfo_Entries(_mb_info),
	     rec = L4_BootInfo_FirstEntry(_mb_info);
	     entries > 0;
	     entries--, rec = L4_Next(rec))
	{
		if (L4_Type(rec) == L4_BootInfo_Module)
			i++;
	}

	/* return count of modules */
	return i;
}


Rom_module Multiboot_info::get_module(unsigned num)
{
	using namespace Pistachio;

	/* find the right record */
	bool found = false;
	unsigned int i = 0;
	L4_Word_t entries;
	L4_BootRec_t *rec;
	for (entries = L4_BootInfo_Entries(_mb_info),
	     rec = L4_BootInfo_FirstEntry(_mb_info);
	     entries > 0;
	     entries--, rec = L4_Next(rec))
	{
		if ((L4_Type(rec) == L4_BootInfo_Module) &&
		    (i++ == num)) {
			found = true;
			break;
		}
	}

	if (!found)
		panic("No such rom module");

	/* strip path info and command line */
	char *name = L4_Module_Cmdline(rec);
	for (char *c = name; *c != 0; c++) {
		if (*c == '/') name = c + 1;
		if (*c == ' ') {
			*c = 0;
			break;
		}
	}

	/* request the memory from sigma0 and create the rom_module object */
	L4_Word_t start = L4_Module_Start(rec);
	L4_Word_t size = L4_Module_Size(rec);

	if (start != trunc_page(start))
		panic("Module is not aligned to page boundary.");

	L4_ThreadId_t s0 = get_sigma0();
	addr_t ps = get_page_size();
	for (addr_t cur = start; cur < start + size; cur += ps) {
		L4_Fpage_t fp = L4_Sigma0_GetPage(s0, L4_Fpage(cur, ps));

		if (L4_IsNilFpage(fp) ||
		 L4_Address(fp) != cur)
			panic("Unable to map module data.");
	}

	Rom_module ret = Rom_module(start, size, name);
	return ret;
}


bool Multiboot_info::check_module(unsigned num, addr_t *start, addr_t *end)
{
	panic("TODO Who calls check_module?");
	return false;
}


Multiboot_info::Multiboot_info(void *mb_info)
: _mb_info(mb_info)
{
	using namespace Pistachio;

	if (!L4_BootInfo_Valid(mb_info))
		panic("Invalid BootInfo.");

	/* some debug info, can probably be removed */
	unsigned int i;
	L4_Word_t entries;
	L4_BootRec_t *rec;
	for (entries = L4_BootInfo_Entries(mb_info),
	     rec = L4_BootInfo_FirstEntry(mb_info),
	     i = 0;
	     entries > 0;
	     entries--, i++, rec = L4_Next(rec)) {

		VPRINTF("Entry[%d]\n", i);
		switch (L4_Type(rec)) {
		case L4_BootInfo_Module:
			VPRINTF(" Type: Module\n");
			VPRINTF(" Cmd : %s\n", L4_Module_Cmdline(rec));
			break;
		case L4_BootInfo_SimpleExec:
			VPRINTF(" Type: SimpleExec (ignored)\n");
			VPRINTF(" Cmd : %s\n", L4_SimpleExec_Cmdline(rec));
			break;
		case L4_BootInfo_EFITables:
			VPRINTF(" Type: EFITables (ignored)\n"); break;
		case L4_BootInfo_Multiboot:
			VPRINTF(" Type: Multiboot (ignored)\n"); break;
		}
	}
}
