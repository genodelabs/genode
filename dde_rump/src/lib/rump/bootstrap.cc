/*
 * \brief  Call initialization functions for all modules and components
 * \author Sebastian Sumpf
 * \date   2013-12-12
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {
#include <sys/types.h>
#include <rump/rumpuser.h>
#include <elf.h>
}

#include <base/env.h>
#include <base/printf.h>
#include <util/string.h>

#include "dl_interface.h"

extern "C" void wait_for_continue();

#ifdef _LP64
typedef Elf64_Dyn Elf_Dyn;
typedef Elf64_Sym Elf_Sym;
#else
typedef Elf32_Dyn Elf_Dyn;
typedef Elf32_Sym Elf_Sym;
#endif

static bool const verbose = false;


static void *dl_main;

struct Sym_tab
{
	link_map   *map;

	void const *dynamic_base = 0;
	void const *sym_base     = 0;

	Elf_Addr    str_tab      = 0;
	size_t      sym_cnt      = 0;
	size_t      out_cnt      = 0;
	size_t      str_size     = 0;

	Elf_Sym    *sym_tab;

	Sym_tab(link_map *map) : map(map), dynamic_base(map->l_ld)
	{
		if (!dynamic_base) {
			PERR("%s: base is bogus %lx", map->l_name, map->l_addr);
			throw -1;
		}

		if (verbose)
			PDBG("for %s at %lx\n", map->l_name, map->l_addr);

		find_tables();

		if (!sym_base) {
			PERR("%s: could not find symbol table (sym_base %p)", map->l_name,
			                                                      sym_base);
			throw -2;
		}

		/* alloc memory for the tables */
		alloc_memory();

		/* file sym_tab and str_tab */
		read_symbols();
	}

	~Sym_tab()
	{
		if (sym_tab)
			destroy(Genode::env()->heap(), sym_tab);
	}


	Elf_Sym const *elf_symbol(int index = 0)
	{
		Elf_Sym const *s = static_cast<Elf_Sym const *>(sym_base);
		return &s[index];
	}

	Elf_Dyn const *elf_dyn(int index = 0)
	{
		Elf_Dyn const *d = static_cast<Elf_Dyn const *>(dynamic_base);
		return &d[index];
	}

	bool is_wanted(char const *name)
	{
		using namespace Genode;
		return (!strcmp(name, "rump", 4) ||
		        !strcmp(name, "RUMP", 4) ||
		        !strcmp(name, "__", 2)) ? true : false; 
	}

	/**
	 * Find symtab and strtab
	 */
	void find_tables()
	{
		uint64_t dyn_tag = elf_dyn()->d_tag;
		for (int i            = 0;
		     dyn_tag != DT_NULL;
		     i++,
		     dyn_tag = elf_dyn(i)->d_tag) {

			switch (dyn_tag) {
				case DT_SYMTAB:
					sym_base = (void *)(elf_dyn(i)->d_un.d_ptr + map->l_addr);
					break;
				case DT_STRTAB:
					str_tab = elf_dyn(i)->d_un.d_ptr + map->l_addr;
					break;
				case DT_STRSZ:
					str_size = elf_dyn(i)->d_un.d_ptr;
					break;
				case DT_HASH:
					{
						Elf_Symindx *hashtab = (Elf_Symindx *)(elf_dyn(i)->d_un.d_ptr +
						                                       map->l_addr);
						sym_cnt = hashtab[1];
					}
					break;
				case DT_SYMENT:
					{
						size_t sym_size = elf_dyn(i)->d_un.d_ptr;
						if (sym_size != sizeof(Elf_Sym))
							PWRN("Elf symbol size does not match binary %zx != elf %zx",
							     sym_size, sizeof(Elf_Sym));
					}
				default:
					break;
			}
		}
	}

	void alloc_memory()
	{
		sym_tab = (Elf_Sym *)Genode::env()->heap()->alloc(sizeof(Elf_Sym) * sym_cnt);
	}

	void read_symbols()
	{
		for (unsigned i = 0; i < sym_cnt; i++) {

			Elf_Sym const *sym = elf_symbol(i);
			if (sym->st_shndx == SHN_UNDEF || !sym->st_value)
				continue;
	
			char const *name = (char const *)sym->st_name + str_tab;
			if (!is_wanted(name))
				continue;

			sym_tab[out_cnt] = *sym;
			/* set absolute value */
			sym_tab[out_cnt].st_value += map->l_addr; 
			if (verbose)
				PDBG("Read symbol %s val: %x", name, sym_tab[out_cnt].st_value);
			out_cnt++;
		}
	}

	void rump_load(rump_symload_fn symload)
	{
		symload(sym_tab, sizeof(Elf_Sym) * out_cnt, (char *)str_tab, str_size);
	}
};


char const *_filename(char const *path)
{
	int i;
	int len = Genode::strlen(path);
	for (i = len; i > 0 && path[i] != '/'; i--) ;
	return path + i + 1;
}

/**
 * Call init functions of libraries
 */
static void _dl_init(link_map const *map,
                    rump_modinit_fn mod_init,
                    rump_compload_fn comp_init)
{
	void *handle = dlopen(map->l_name, RTLD_LAZY);
	if (!handle)
		PERR ("Could not dlopen %s\n", map->l_name);

	struct modinfo **mi_start, **mi_end;
	struct rump_component **rc_start, **rc_end;

	mi_start = (modinfo **)dlsym(handle, "__start_link_set_modules");
	mi_end   = (modinfo **)dlsym(handle, "__stop_link_set_modules");
	if (verbose)
		PDBG("MI: start: %p end: %p", mi_start, mi_end);
	if (mi_start && mi_end)
		mod_init(mi_start, (size_t)(mi_end-mi_start));

	rc_start = (rump_component **)dlsym(handle, "__start_link_set_rump_components");
	rc_end   = (rump_component **)dlsym(handle, "__stop_link_set_rump_components");
	if (verbose)
		PDBG("RC: start: %p end: %p", rc_start, rc_end);
	if (rc_start && rc_end) {
		for (; rc_start < rc_end; rc_start++)
			comp_init(*rc_start);
	}
}


void rumpuser_dl_bootstrap(rump_modinit_fn domodinit, rump_symload_fn symload,
                           rump_compload_fn compload)
{
	/* open main program and request link map */
	dl_main = dlopen(0, RTLD_NOW);

	struct link_map *map;
	if(dlinfo(dl_main, RTLD_DI_LINKMAP, &map)) {
		PERR("Error: Could not retrieve linkmap from main program");
		return;
	}

	for (; map->l_next; map = map->l_next) ;

	struct link_map *curr_map;

	for (curr_map = map; curr_map; curr_map = curr_map->l_prev)
		if (!Genode::strcmp(_filename(curr_map->l_name), "rump", 4)) {
			Sym_tab tab(curr_map);
			/* load into rum kernel */
			tab.rump_load(symload);
			/* init modules and components */
			_dl_init(curr_map, domodinit, compload);
		}
	PINF("BOOTSTRAP");
}


void * rumpuser_dl_globalsym(const char *symname)
{

	void *addr = dlsym(RTLD_DEFAULT, symname);

	if (verbose)
		PDBG("Lookup: %s addr %p", symname, addr);

	return addr;
}
