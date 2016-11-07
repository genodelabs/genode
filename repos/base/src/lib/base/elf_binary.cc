/**
 * \brief  ELF binary utility
 * \author Christian Helmuth
 * \date   2006-05-04
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* base-internal includes */
#include <base/internal/elf_format.h>
#include <base/internal/elf.h>

using namespace Genode;


int Elf_binary::_ehdr_check_compat()
{
	Elf_Ehdr *ehdr = (Elf_Ehdr *)_start;

	if (memcmp(ehdr, ELFMAG, SELFMAG) != 0) {
		error("binary is not an ELF");
		return -1;
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS) {
		error("support for 32/64-bit objects only");
		return -1;
	}

	/* start executeables and shared objects with entry points only */
	if (!(ehdr->e_type == ET_EXEC || (ehdr->e_type == ET_DYN && ehdr->e_entry))) {
		error("program is no executable");
		return -1;
	}

	return 0;
}


int Elf_binary::_ph_table_check_compat()
{
	Elf_Phdr *ph_table = (Elf_Phdr *)_ph_table;
	unsigned num = _phnum;
	unsigned i;

	for (i = 0; i < num; i++) {
		if (ph_table[i].p_type == PT_LOAD)
			if (ph_table[i].p_align & (0x1000 - 1)) {
				warning("unsupported alignment ", (unsigned long) ph_table[i].p_align);
				return -1;
			}
		if (ph_table[i].p_type == PT_DYNAMIC)
			_dynamic = true;

		if (ph_table[i].p_type == PT_INTERP) {
			Elf_Phdr *phdr = &((Elf_Phdr *)_ph_table)[i];
			char *interp = (char *)(_start + phdr->p_offset);

			if (!strcmp(interp, "ld.lib.so"))
				_interp = true;
		}
	}

	return 0;
}


Elf_segment Elf_binary::get_segment(unsigned num)
{
	void *start;
	size_t offset, filesz, memsz;
	Elf_binary::Flags flags = { 0, 0, 0, 0 };

	if (!valid()) return Elf_segment();

	if (!(num < _phnum)) return Elf_segment();

	Elf_Phdr *phdr = &((Elf_Phdr *)_ph_table)[num];

	start  = (void *)phdr->p_vaddr;
	offset = phdr->p_offset;
	filesz = phdr->p_filesz;
	memsz  = phdr->p_memsz;

	flags.r = (phdr->p_flags & PF_R) ? 1 : 0;
	flags.w = (phdr->p_flags & PF_W) ? 1 : 0;
	flags.x = (phdr->p_flags & PF_X) ? 1 : 0;

	/*
	 * Skip loading of ELF segments that are not PT_LOAD or have no memory
	 * size.
	 */
	if (phdr->p_type != PT_LOAD || !memsz)
		flags.skip = 1;

	return Elf_segment(this, start, offset, filesz, memsz, flags);
}


Elf_binary::Elf_binary(addr_t start)
: _valid(false), _dynamic(false), _interp(false), _start(start)
{
	Elf_Ehdr *ehdr = (Elf_Ehdr *)start;

	/* check for unsupported ELF features */
	if (_ehdr_check_compat()) return;

	/* program entry point */
	if (!(_entry = ehdr->e_entry)) return;

	/* segment tables */
	_ph_table  = _start + ehdr->e_phoff;
	_phentsize = ehdr->e_phentsize;
	_phnum     = ehdr->e_phnum;

	/* program segments */
	if (_ph_table_check_compat()) return;

	/* ready to rock */
	_valid = true;
}
