/**
 * \brief  ELF loading/unloading support
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <linker.h>
#include <base/internal/page_size.h>

/* Genode includes */
#include <util/construct_at.h>
#include <base/allocator_avl.h>
#include <base/env.h>
#include <os/attached_rom_dataspace.h>
#include <region_map/client.h>
#include <util/retry.h>

char const *Linker::ELFMAG = "\177ELF";

using namespace Linker;
using namespace Genode;

namespace Linker
{
	class  Rm_area;
	struct Elf_file;
}

/**
 * Managed dataspace for ELF files (singelton)
 */
class Linker::Rm_area
{
	public:

		typedef Region_map_client::Local_addr      Local_addr;
		typedef Region_map_client::Region_conflict Region_conflict;

	private:

		Region_map_client _rm;

		addr_t        _base;  /* base address of dataspace */
		Allocator_avl _range; /* VM range allocator */

	protected:

		Rm_area(addr_t base)
		: _rm(env()->pd_session()->linker_area()), _range(env()->heap())
		{
			_base = (addr_t) env()->rm_session()->attach_at(_rm.dataspace(), base);
			_range.add_range(base, Pd_session::LINKER_AREA_SIZE);
		}

	public:

		static Rm_area *r(addr_t base = 0)
		{
			/*
			 * The capabilities in this class become invalid when doing a
			 * fork in the noux environment. Hence avoid destruction of
			 * the singleton object as the destructor would try to access
			 * the capabilities also in the forked process.
			 */
			static bool constructed = 0;
			static char placeholder[sizeof(Rm_area)];
			if (!constructed) {
				construct_at<Rm_area>(placeholder, base);
				constructed = 1;
			}
			return reinterpret_cast<Rm_area *>(placeholder);
		}

		/**
		 * Reserve VM region of 'size' at 'vaddr'. Allocate any free region if
		 * 'vaddr' is zero
		 */
		addr_t alloc_region(size_t size, addr_t vaddr = 0)
		{
			addr_t addr = vaddr;

			if (addr && (_range.alloc_addr(size, addr).error()))
				throw Region_conflict();
			else if (!addr &&
			         _range.alloc_aligned(size, (void **)&addr,
			                              get_page_size_log2()).error())
			{
				throw Region_conflict();
			}

			return addr;
		}

		void free_region(addr_t vaddr) { _range.free((void *)vaddr); }

		/**
		 * Overwritten from 'Region_map_client'
		 */
		Local_addr attach_at(Dataspace_capability ds, addr_t local_addr,
		                     size_t size = 0, off_t offset = 0)
		{
			return retry<Region_map::Out_of_metadata>(
				[&] () {
					return _rm.attach_at(ds, local_addr - _base, size, offset);
				},
				[&] () { env()->parent()->upgrade(env()->pd_session_cap(), "ram_quota=8K"); });
		}

		/**
		 * Overwritten from 'Region_map_client'
		 */
		Local_addr attach_executable(Dataspace_capability ds, addr_t local_addr,
		                             size_t size = 0, off_t offset = 0)
		{
			return retry<Region_map::Out_of_metadata>(
				[&] () {
					return _rm.attach_executable(ds, local_addr - _base, size, offset);
				},
				[&] () { env()->parent()->upgrade(env()->pd_session_cap(), "ram_quota=8K"); });
		}

		void detach(Local_addr local_addr) { _rm.detach((addr_t)local_addr - _base); }
};


/**
 * Map ELF files
 */
struct Linker::Elf_file : File
{
	Rom_connection           rom;
	Ram_dataspace_capability ram_cap[Phdr::MAX_PHDR];
	bool                     loaded;
	Elf_file(char const *name, bool load = true)
	:
	 rom(name), loaded(load)
	{
		load_phdr();

		if (load)
			load_segments();
	}

	virtual ~Elf_file()
	{
		if (loaded)
			unload_segments();
	}

	/**
	 * Check if ELF is sane
	 */
	bool check_compat(Elf::Ehdr const *ehdr)
	{
		if (memcmp(ehdr, ELFMAG, SELFMAG) != 0) {
			Genode::error("LD: binary is not an ELF");
			return false;
		}

		if (ehdr->e_ident[EI_CLASS] != ELFCLASS) {
			Genode::error("LD: support for 32/64-bit objects only");
			return false;
		}

		return true;
	}

	/**
	 * Copy program headers and read entry point
	 */
	void load_phdr()
	{
		Elf::Ehdr *ehdr = (Elf::Ehdr *)env()->rm_session()->attach(rom.dataspace(), 0x1000);

		if (!check_compat(ehdr))
			throw Incompatible();

		/* set entry point and program header information */
		phdr.count = ehdr->e_phnum;
		entry      = (Entry)ehdr->e_entry;

		/* copy program headers */
		addr_t header = (addr_t)ehdr + ehdr->e_phoff;
		for (unsigned i = 0; i < phdr.count; i++, header += ehdr->e_phentsize)
			memcpy(&phdr.phdr[i], (void *)header, ehdr->e_phentsize);

		env()->rm_session()->detach(ehdr);

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
		for (unsigned i = 0; i < phdr.count; i++) {
			Elf::Phdr *ph = &phdr.phdr[i];

			if (ph->p_type != PT_LOAD)
				continue;

			if (ph->p_align & (0x1000 - 1)) {
				Genode::error("LD: Unsupported alignment ", (void *)ph->p_align);
				throw Incompatible();
			}

			result.phdr[result.count++] = *ph;
		}
	}

	bool is_rx(Elf::Phdr const &ph) {
		return ((ph.p_flags & PF_MASK) == (PF_R | PF_X)); }

	bool is_rw(Elf::Phdr const &ph) {
		return ((ph.p_flags & PF_MASK) == (PF_R | PF_W)); }

	/**
	 * Load PT_LOAD segments
	 */
	void load_segments()
	{
		Phdr p;

		/* search for PT_LOAD */
		loadable_segments(p);

		/* allocate region */
		reloc_base = Rm_area::r(start)->alloc_region(size, start);
		reloc_base = (start == reloc_base) ?  0 : reloc_base;

		if (verbose_loading)
			Genode::log("LD: reloc_base: ", Genode::Hex(reloc_base),
			            " start: ", Genode::Hex(start),
			            " end: ", Genode::Hex(reloc_base + start + size));

		for (unsigned i = 0; i < p.count; i++) {
			Elf::Phdr *ph = &p.phdr[i];

			if (is_rx(*ph))
				load_segment_rx(*ph);

			else if (is_rw(*ph))
				load_segment_rw(*ph, i);

			else {
				Genode::error("LD: Non-RW/RX segment");
				throw Invalid_file();
			}
		}
	}

	/**
	 * Map read-only segment
	 */
	void load_segment_rx(Elf::Phdr const &p)
	{
		Rm_area::r()->attach_executable(rom.dataspace(),
		                                trunc_page(p.p_vaddr) + reloc_base,
		                                round_page(p.p_memsz),
		                                trunc_page(p.p_offset));
	}

	/**
	 * Copy read-write segment
	 */
	void load_segment_rw(Elf::Phdr const &p, int nr)
	{
		void  *src = env()->rm_session()->attach(rom.dataspace(), 0, p.p_offset);
		addr_t dst = p.p_vaddr + reloc_base;

		ram_cap[nr] = env()->ram_session()->alloc(p.p_memsz);
		Rm_area::r()->attach_at(ram_cap[nr], dst);

		memcpy((void*)dst, src, p.p_filesz);

		/* clear if file size < memory size */
		if (p.p_filesz < p.p_memsz)
			memset((void *)(dst + p.p_filesz), 0, p.p_memsz - p.p_filesz);

		env()->rm_session()->detach(src);
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
			Rm_area::r()->detach(trunc_page(p.phdr[i].p_vaddr) + reloc_base);

		/* free region from RM area */
		Rm_area::r()->free_region(trunc_page(p.phdr[0].p_vaddr) + reloc_base);

		/* free ram of RW segments */
		for (unsigned i = 0; i < Phdr::MAX_PHDR; i++)
			if (ram_cap[i].valid()) {
				env()->ram_session()->free(ram_cap[i]);
			}
	}
};


File const *Linker::load(char const *path, bool load)
{
	if (verbose_loading)
		Genode::log("LD loading: ", path, " "
		            "(PHDRS only: ", load ? "no" : "yes", ")");

	Elf_file *file = new (env()->heap()) Elf_file(Linker::file(path), load);
	return file;
}

