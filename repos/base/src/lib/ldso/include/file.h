/*
 * \brief  ELF file setup
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FILE_H_
#define _INCLUDE__FILE_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <util.h>
#include <debug.h>
#include <region_map.h>


namespace Linker {

	struct Phdr;
	struct File;
	struct Elf_file;

	static inline bool is_rx(Elf::Phdr const &ph) {
		return ((ph.p_flags & PF_MASK) == (PF_R | PF_X)); }

	static inline bool is_rw(Elf::Phdr const &ph) {
		return ((ph.p_flags & PF_MASK) == (PF_R | PF_W)); }
}


/**
 * Program header
 */
struct Linker::Phdr
{
	enum { MAX_PHDR = 10 };

	Elf::Phdr  phdr[MAX_PHDR];
	uint16_t   count = 0;
};


/**
 * ELF file info
 */
struct Linker::File
{
	typedef void (*Entry)(void);

	Phdr       phdr       { };
	Entry      entry      { };
	Elf::Addr  reloc_base { 0 };
	Elf::Addr  start      { 0 };
	Elf::Size  size       { 0 };

	virtual ~File() { }

	Elf::Phdr const *elf_phdr(unsigned index) const
	{
		if (index < phdr.count)
			return &phdr.phdr[index];

		return 0;
	}

	unsigned elf_phdr_count() const { return phdr.count; }

	template <typename FN>
	void with_rw_phdr(FN const &fn) const
	{
		for (unsigned i = 0; i < phdr.count; i++) {
			if (is_rw(phdr.phdr[i])) {
				fn(phdr.phdr[i]);
				return;
			}
		}
	}
};


/**
 * Mapped ELF file
 */
struct Linker::Elf_file : File
{
	Env                          &env;
	Constructible<Rom_connection> rom_connection { };
	Rom_dataspace_capability      rom_cap        { };
	Ram_dataspace_capability      ram_cap[Phdr::MAX_PHDR];
	bool                    const loaded;

	typedef String<64> Name;

	Rom_dataspace_capability _rom_dataspace(Name const &name)
	{
		/* request the linker and binary from the component environment */
		Session_capability cap;
		if (name == binary_name())
			cap = env.parent().session_cap(Parent::Env::binary());
		if (name == linker_name())
			cap = env.parent().session_cap(Parent::Env::linker());
		if (cap.valid()) {
			Rom_session_client client(reinterpret_cap_cast<Rom_session>(cap));
			return client.dataspace();
		}

		rom_connection.construct(env, name.string());
		return rom_connection->dataspace();
	}

	void _allocate_region_within_linker_area(Name const &name)
	{
		bool const binary = (start != 0);

		if (binary) {
			Region_map::r()->alloc_region_at(size, start);
			reloc_base = 0;
			return;
		}

		/*
		 * Assign libraries that must stay resident during 'execve' to
		 * the end of the linker area to ensure that the newly loaded
		 * binary has enough room within the linker area.
		 */
		bool const resident = (name == "libc.lib.so")
		                   || (name == "libm.lib.so")
		                   || (name == "posix.lib.so")
		                   || (strcmp(name.string(), "vfs", 3) == 0);

		reloc_base = resident ? Region_map::r()->alloc_region_at_end(size)
		                      : Region_map::r()->alloc_region(size);
		start = 0;
	}

	Elf_file(Env &env, Allocator &md_alloc, Name const &name, bool load)
	:
		env(env), rom_cap(_rom_dataspace(name)), loaded(load)
	{
		load_phdr();

		/*
		 * Initialize the linker area at the link address of the binary,
		 * which happens to be the first loaded 'Elf_file'.
		 *
		 * XXX Move this initialization to the linker's 'construct' function
		 *     once we use relocatable binaries.
		 */
		if (load && !Region_map::r().constructed())
			Region_map::r().construct(env, md_alloc, start);

		if (load) {
			_allocate_region_within_linker_area(name);
			load_segments();
		}
	}

	virtual ~Elf_file()
	{
		if (loaded)
			unload_segments();
	}

	/**
	 * Check if ELF is sane
	 */
	bool check_compat(Elf::Ehdr const &ehdr)
	{
		char const * const ELFMAG = "\177ELF";
		if (memcmp(&ehdr, ELFMAG, SELFMAG) != 0) {
			error("LD: binary is not an ELF");
			return false;
		}

		if (ehdr.e_ident[EI_CLASS] != ELFCLASS) {
			error("LD: support for 32/64-bit objects only");
			return false;
		}

		return true;
	}

	/**
	 * Copy program headers and read entry point
	 */
	void load_phdr()
	{
		{
			/* temporary map the binary to read the program header */
			Attached_dataspace ds(env.rm(), rom_cap);

			Elf::Ehdr const &ehdr = *ds.local_addr<Elf::Ehdr>();

			if (!check_compat(ehdr))
				throw Incompatible();

			/* set entry point and program header information */
			phdr.count = ehdr.e_phnum;
			entry      = (Entry)ehdr.e_entry;

			/* copy program headers */
			addr_t header = (addr_t)&ehdr + ehdr.e_phoff;
			for (unsigned i = 0; i < phdr.count; i++, header += ehdr.e_phentsize)
				memcpy(&phdr.phdr[i], (void *)header, ehdr.e_phentsize);
		}

		Phdr p;
		loadable_segments(p);
		/* start vaddr */
		start         = trunc_page(p.phdr[0].p_vaddr);
		Elf::Phdr *ph = &p.phdr[p.count - 1];
		/* size of lodable segments */
		size          = round_page(ph->p_vaddr + ph->p_memsz) - start;
	}

	/**
	 * Find PT_LOAD segemnts
	 */
	void loadable_segments(Phdr &result)
	{
		bool dynamic = false;
		for (unsigned i = 0; i < phdr.count; i++) {
			Elf::Phdr *ph = &phdr.phdr[i];

			if (ph->p_type == PT_DYNAMIC)
				dynamic = true;

			if (ph->p_type != PT_LOAD)
				continue;

			if (ph->p_align & (0x1000 - 1)) {
				error("LD: Unsupported alignment ", (void *)ph->p_align);
				throw Incompatible();
			}

			result.phdr[result.count++] = *ph;
		}

		/*
		 * Check for 'DYNAMIC' segment which should be present in all dynamic ELF
		 * files.
		 */
		if (!dynamic) {
			error("LD: ELF without DYNAMIC segment appears to be statically linked ",
			      "(ld=\"no\")");
			throw Incompatible();
		}
	}

	/**
	 * Load PT_LOAD segments
	 */
	void load_segments()
	{
		Phdr p;

		/* search for PT_LOAD */
		loadable_segments(p);

		if (verbose_loading)
			log("LD: reloc_base: ", Hex(reloc_base),
			    " start: ",         Hex(start),
			    " end: ",           Hex(reloc_base + start + size));

		for (unsigned i = 0; i < p.count; i++) {
			Elf::Phdr *ph = &p.phdr[i];

			if (is_rx(*ph))
				load_segment_rx(*ph);

			else if (is_rw(*ph))
				load_segment_rw(*ph, i);

			else {
				error("LD: Non-RW/RX segment");
				throw Invalid_file();
			}
		}
	}

	/**
	 * Map read-only segment
	 */
	void load_segment_rx(Elf::Phdr const &p)
	{
		Region_map::r()->attach_executable(rom_cap,
		                                   trunc_page(p.p_vaddr) + reloc_base,
		                                   round_page(p.p_memsz),
		                                   trunc_page(p.p_offset));
	}

	/**
	 * Copy read-write segment
	 */
	void load_segment_rw(Elf::Phdr const &p, int nr)
	{
		void  *src = env.rm().attach(rom_cap, 0, p.p_offset);
		addr_t dst = p.p_vaddr + reloc_base;

		ram_cap[nr] = env.ram().alloc(p.p_memsz);
		Region_map::r()->attach_at(ram_cap[nr], dst);

		memcpy((void*)dst, src, p.p_filesz);

		/* clear if file size < memory size */
		if (p.p_filesz < p.p_memsz)
			memset((void *)(dst + p.p_filesz), 0, p.p_memsz - p.p_filesz);

		env.rm().detach(src);
	}

	/**
	 * Unmap segements, RM regions, and free allocated dataspaces
	 */
	void unload_segments()
	{
		Phdr p;
		loadable_segments(p);

		/* detach from RM area */
		for (unsigned i = 0; i < p.count; i++)
			Region_map::r()->detach(trunc_page(p.phdr[i].p_vaddr) + reloc_base);

		/* free region from RM area */
		Region_map::r()->free_region(trunc_page(p.phdr[0].p_vaddr) + reloc_base);

		/* free ram of RW segments */
		for (unsigned i = 0; i < Phdr::MAX_PHDR; i++)
			if (ram_cap[i].valid()) {
				env.ram().free(ram_cap[i]);
			}
	}
};

#endif /* _INCLUDE__FILE_H_ */
