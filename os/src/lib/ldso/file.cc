/*
 * \brief  libc file handling function emulation (open/read/write/mmap)
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <ldso/arch.h>
#include <rom_session/connection.h>
#include <rm_session/connection.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <util/list.h>

#include "file.h"

extern int debug;

namespace Genode {

	/**
	 * Managed dataspace for ELF files (singelton)
	 */
	class Rm_area : public Rm_connection
	{
		private:

			/* size of dataspace */
			enum { RESERVATION = 128 * 1024 * 1024 };

			addr_t        _base;  /* base address of dataspace */
			Allocator_avl _range; /* VM range allocator */

			Rm_area(addr_t base)
			: Rm_connection(0, RESERVATION), _range(env()->heap())
			{
				_base = (addr_t) env()->rm_session()->attach_at(dataspace(), base);
				_range.add_range(base, RESERVATION);
			}

		public:

			static Rm_area *r(addr_t base = 0)
			{
				static Rm_area _area(base);
				return &_area;
			}

			/**
			 * Reserve VM region of 'size' at 'vaddr'. Allocate any free region if
			 * 'vaddr' is zero
			 */
			addr_t alloc_region(size_t size, addr_t vaddr = 0)
			{
				addr_t addr = vaddr;

				if (addr && (_range.alloc_addr(size, addr) != Range_allocator::ALLOC_OK))
					throw Region_conflict();
				else if (!addr && !_range.alloc_aligned(size, (void **)&addr, 12))
					throw Region_conflict();

				return addr;
			}

			void free_region(addr_t vaddr) { _range.free((void *)vaddr); }

			/**
			 * Overwritten from 'Rm_connection'
			 */
			Local_addr attach_at(Dataspace_capability ds, addr_t local_addr,
			                     size_t size = 0, off_t offset = 0) {
				return Rm_connection::attach_at(ds, local_addr - _base, size, offset); }

			/**
			 * Overwritten from 'Rm_connection'
			 */
			Local_addr attach_executable(Dataspace_capability ds, addr_t local_addr,
			                             size_t size = 0, off_t offset = 0) {
				return Rm_connection::attach_executable(ds, local_addr - _base, size, offset); }

			void detach(Local_addr local_addr) {
				Rm_connection::detach((addr_t)local_addr - _base); }
	};


	class Fd_handle : public List<Fd_handle>::Element
	{
		private:

			addr_t                   _vaddr;  /* image start */
			addr_t                   _daddr;  /* data start */
			Rom_dataspace_capability _ds_rom; /* image ds */
			Ram_dataspace_capability _ds_ram; /* data ds */
			int                      _fd;     /* file handle */

		public:

			enum {
				ENOT_FOUND = 1
			};

			Fd_handle(int fd, Rom_dataspace_capability ds_rom)
			: _vaddr(~0UL),  _ds_rom(ds_rom), _fd(fd)
			{}

			addr_t                   vaddr()      { return _vaddr; }
			Rom_dataspace_capability dataspace()  { return _ds_rom; }

			void setup_data(addr_t vaddr, addr_t vlimit, addr_t flimit, off_t offset)
			{
				/* allocate data segment */
				_ds_ram = env()->ram_session()->alloc(vlimit - vaddr);
				Rm_area::r()->attach_at(_ds_ram, vaddr);

				/* map rom data segment */
				void *rom_data = env()->rm_session()->attach(_ds_rom, 0, offset);
				
				/* copy data */
				memcpy((void *)vaddr, rom_data, flimit - vaddr);
				env()->rm_session()->detach(rom_data);

				/* set parent cap (arch.lib.a) */
				set_parent_cap_arch((void *)vaddr);

				_daddr = vaddr;
			}

			void setup_text(addr_t vaddr, size_t size, off_t offset)
			{
				_vaddr = vaddr;
				Rm_area::r()->attach_executable(_ds_rom, vaddr, size, offset);
			}

			addr_t alloc_region(addr_t vaddr, addr_t vlimit)
			{
				Rm_area *a = Rm_area::r(vaddr);
				return a->alloc_region(vlimit - vaddr, vaddr);
			}

			static List<Fd_handle> *file_list()
			{
				static List<Fd_handle> _file_list;
				return &_file_list;
			}

			static Fd_handle *find_handle(int fd)
			{
				Fd_handle *h = file_list()->first();

				while (h) {
					if (h->_fd == fd)
						return h;
					h = h->next();
				}
				throw ENOT_FOUND;
			}

			static void free(void *addr)
			{
				addr_t vaddr = (addr_t) addr;

				Fd_handle *h = file_list()->first();

				while (h) {

					if (h->_vaddr != vaddr) {
						h = h->next();
						continue;
					}

					destroy(env()->heap(), h);
					return;
				}
			}
	
			~Fd_handle()
			{
				file_list()->remove(this);

				if (_vaddr != ~0UL) {
					Rm_area::r()->detach(_vaddr);
					Rm_area::r()->detach(_daddr);
					Rm_area::r()->free_region(_vaddr);
					env()->ram_session()->free(_ds_ram);
				}
			}
	};
}


extern "C"int open(const char *pathname, int flags)
{
	using namespace Genode;
	static int fd = -1;

	/* skip directory part from pathname, leaving only the plain filename */
	const char *filename = pathname;
	for (; *pathname; pathname++)
		if (*pathname == '/')
			filename = pathname + 1;

	try {
		/* open the file dataspace and attach it */
		Rom_connection rom(filename);
		rom.on_destruction(Rom_connection::KEEP_OPEN);

		Fd_handle::file_list()->insert(new(env()->heap())
		                               Fd_handle(++fd, rom.dataspace()));
		return fd;
	}
	catch (...) {
		PERR("Could not open %s\n", filename);
	}

	return -1;
}


extern "C" int find_binary_name(int fd, char *buf, size_t buf_size)
{
	using namespace Genode;

	Fd_handle *h;
	try {
		h = Fd_handle::find_handle(fd);
	}
	catch (...) {
		PERR("handle not found\n");
		return -1;
	}

	return binary_name(h->dataspace(), buf, buf_size);
}


extern "C" ssize_t read(int fd, void *buf, size_t count)
{
	using namespace Genode;

	Fd_handle *h;
	try {
		h = Fd_handle::find_handle(fd);
	}
	catch (...) {
		PERR("handle not found\n");
		return -1;
	}
	
	try {
		void *base = env()->rm_session()->attach(h->dataspace(), count);
		memcpy(buf, base, count);
		env()->rm_session()->detach(base);
	}
	catch (...) {
		return -1;
	}

	return count;
}


extern "C" ssize_t write(int fd, const void *buf, size_t count) 
{
	Genode::printf("%p", buf);
	return count;
}


extern "C" int munmap(void *addr, size_t /* length */) 
{
	using namespace Genode;
	Fd_handle::free(addr);
	return 0;
}


extern "C" void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	using namespace Genode;

	if(!(flags & MAP_ANON)) {
		PERR("No MAP_ANON");
		return MAP_FAILED;
	}

	/* called during ldso relocation */
	if (flags & MAP_LDSO) {
		enum { MEM_SIZE = 32 * 1024 };
		static char _mem[MEM_SIZE];

		/* generate fault on allocation */
		if (length > MEM_SIZE) {
			int *fault = (int *)0xa110ce88;
			*fault = 1;
		}
		return _mem;
	}
	/* memory allocation */
	else {
		void *base;

		try {
			Ram_dataspace_capability ds_cap = env()->ram_session()->alloc(round_page(length));
			base = env()->rm_session()->attach(ds_cap, length, 0,
			                                   (addr) ? true : false, (addr_t)addr);
		}
		catch(...) {
			PERR("Anonmymous mmap failed\n");
			return MAP_FAILED;
		}

		if (debug)
			PDBG("base %p", base);

		return base;
	}

	return MAP_FAILED;
}


extern "C" void *genode_map(int fd, Elf_Phdr **segs)
{
	using namespace Genode;

	if (1 > 1) {
		PERR("More than two segments in ELF");
		return MAP_FAILED;
	}

	Fd_handle *h;
	try { h = Fd_handle::find_handle(fd); }
	catch (...) {
		PERR("handle not found\n");
		return MAP_FAILED;
	}

	addr_t base_vaddr  = trunc_page(segs[0]->p_vaddr);
	addr_t base_offset = trunc_page(segs[0]->p_offset);
	addr_t base_msize  = round_page(segs[1]->p_vaddr - base_vaddr);
	addr_t base_vlimit = round_page(segs[1]->p_vaddr + segs[1]->p_memsz);
	/* is this a fixed address */
	bool   fixed       = base_vaddr ? true : false;

	try {
		base_vaddr = h->alloc_region(base_vaddr, base_vlimit);
	} catch (...) {
		PERR("Region allocation failed: %lx-%lx", base_vaddr, base_vlimit);
		return MAP_FAILED;
	}

	/* map text segment */
	h->setup_text(base_vaddr, base_msize, base_offset);

	addr_t offset      = fixed ? 0 : base_vaddr;
	base_vlimit       += offset;
	addr_t base_flimit = offset + segs[1]->p_vaddr + segs[1]->p_filesz;
	base_vaddr         = offset + trunc_page(segs[1]->p_vaddr);
	base_offset        = trunc_page(segs[1]->p_offset);
	
	/* copy data segment */
	h->setup_data(base_vaddr, base_vlimit, base_flimit, base_offset);

	return (void *)h->vaddr();
}

