/**
 * \brief  GCC excption handling support
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <linker.h>


using namespace Linker;

/*********
 ** x86 **
 *********/

/**
 * "Walk through shared objects" support, see man page of 'dl_iterate_phdr'
 */
struct Phdr_info
{
	Elf::Addr        addr;      /* module relocation base */
	char const      *name;      /* module name */
	Elf::Phdr const *phdr;      /* pointer to module's phdr */
	Elf::Half        phnum;     /* number of entries in phdr */
};


extern "C" int dl_iterate_phdr(int (*callback) (Phdr_info *info, size_t size, void *data), void *data)
{
	int err = 0;
	Phdr_info info;

	Lock::Guard guard(lock());

	for (Object *e = obj_list_head();e; e = e->next_obj()) {

		info.addr  = e->reloc_base();
		info.name  = e->name();
		info.phdr  = e->file()->phdr.phdr;
		info.phnum = e->file()->phdr.count;

		if (verbose_exception)
			log(e->name(), " reloc ", Hex(e->reloc_base()));

		if ((err = callback(&info, sizeof(Phdr_info), data)))
			break;
	}

	return err;
}


/*********
 ** ARM **
 *********/

/**
 * Return EXIDX program header
 */
static Elf::Phdr const *phdr_exidx(File const *file)
{
	for (unsigned i = 0; i < file->elf_phdr_count(); i++) {
		Elf::Phdr const *ph = file->elf_phdr(i);

		if (ph->p_type == PT_ARM_EXIDX)
			return ph;
	}

	return 0;
}


/**
 * Find ELF and exceptions table segment that that is located under 'pc',
 * address of exception table and number of entries 'pcount'
 */
extern "C" unsigned long dl_unwind_find_exidx(unsigned long pc, int *pcount)
{
	/* size of exception table entries */
	enum { EXIDX_ENTRY_SIZE = 8 };
	*pcount = 0;

	for (Object *e = obj_list_head(); e; e = e->next_obj()) {

		/* address of first PT_LOAD header */
		addr_t base = e->reloc_base() + e->file()->start;

		/* is the 'pc' somewhere within this ELF image */
		if ((pc < base) || (pc >= base + e->file()->size))
			continue;

		/* retrieve PHDR of exception-table segment */
		Elf::Phdr const *exidx = phdr_exidx(e->file());
		if (!exidx)
			continue;

		*pcount = exidx->p_memsz / EXIDX_ENTRY_SIZE;
		return exidx->p_vaddr + e->reloc_base();
	}

	return 0;
}

