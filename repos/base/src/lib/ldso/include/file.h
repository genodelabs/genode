/**
 * \brief  ELF file setup
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FILE_H_
#define _INCLUDE__FILE_H_

namespace Linker {
	struct Phdr;
	struct File;

	/**
	 * Load object
	 *
	 * \param path  Path of object
	 * \param load  True, load binary; False, load ELF header only
	 * 
	 * \throw Invalid_file     Segment is neither read-only/executable or read/write
	 * \throw Region_conflict  There is already something at the given address
	 * \throw Incompatible     Not an ELF
	 *
	 * \return File descriptor
	 */
	File const *load(char const *path, bool load = true); 
}


/**
 * Program header
 */
struct Linker::Phdr
{
	enum { MAX_PHDR = 10 };

	Elf::Phdr  phdr[MAX_PHDR];
	unsigned   count = 0;
};


/**
 * Loaded ELF file
 */
struct Linker::File
{
	typedef void (*Entry)(void);

	Phdr       phdr;
	Entry      entry;
	Elf::Addr  reloc_base = 0;
	Elf::Addr  start      = 0;
	Elf::Size  size       = 0;

	virtual ~File() { }

	Elf::Phdr const *elf_phdr(unsigned index) const
	{
		if (index < phdr.count)
			return &phdr.phdr[index];

		return 0;
	}

	unsigned elf_phdr_count() const { return phdr.count; }
};

#endif /* _INCLUDE__FILE_H_ */
