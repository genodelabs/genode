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

#include <rump/bootstrap.h>
#include <base/log.h>
#include <base/shared_object.h>
#include <util/string.h>

extern "C" void wait_for_continue();

#ifdef _LP64
typedef Elf64_Dyn Elf_Dyn;
typedef Elf64_Sym Elf_Sym;
#else
typedef Elf32_Dyn Elf_Dyn;
typedef Elf32_Sym Elf_Sym;
#endif

static bool const verbose = false;


static Genode::Shared_object *obj_main;
static Genode::Env           *env_ptr;
static Genode::Allocator     *heap_ptr;


void rump_bootstrap_init(Genode::Env &env, Genode::Allocator &alloc)
{
	/* ignore subsequent calls */
	if (env_ptr)
		return;

	env_ptr  = &env;
	heap_ptr = &alloc;
}


/**
 * Exception type
 */
class Missing_call_of_rump_bootstrap_init { };


static Genode::Env &env()
{
	if (!env_ptr)
		throw Missing_call_of_rump_bootstrap_init();

	return *env_ptr;
}


static Genode::Allocator &heap()
{
	if (!heap_ptr)
		throw Missing_call_of_rump_bootstrap_init();

	return *heap_ptr;
}



struct Sym_tab
{
	Genode::Shared_object::Link_map const *map;

	void const *dynamic_base = nullptr;
	void const *sym_base     = nullptr;

	Elf_Addr    str_tab      = 0;
	size_t      sym_cnt      = 0;
	size_t      out_cnt      = 0;
	size_t      str_size     = 0;

	Elf_Sym    *sym_tab;

	Sym_tab(Genode::Shared_object::Link_map const *map)
	: map(map), dynamic_base(map->dynamic)
	{
		if (!dynamic_base) {
			Genode::error(map->path, ": base is bogus ", Genode::Hex(map->addr));
			throw -1;
		}

		if (verbose)
			Genode::log("for ", map->path, " at ", Genode::Hex(map->addr));

		find_tables();

		if (!sym_base) {
			Genode::error(map->path, ": could not find symbol table "
			              "(sym_base ", sym_base, ")");
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
					sym_base = (void *)(elf_dyn(i)->d_un.d_ptr + map->addr);
					break;
				case DT_STRTAB:
					str_tab = elf_dyn(i)->d_un.d_ptr + map->addr;
					break;
				case DT_STRSZ:
					str_size = elf_dyn(i)->d_un.d_ptr;
					break;
				case DT_HASH:
					{
						Elf_Symindx *hashtab = (Elf_Symindx *)(elf_dyn(i)->d_un.d_ptr +
						                                       map->addr);
						sym_cnt = hashtab[1];
					}
					break;
				case DT_SYMENT:
					{
						size_t sym_size = elf_dyn(i)->d_un.d_ptr;
						if (sym_size != sizeof(Elf_Sym))
							Genode::warning("Elf symbol size does not match binary ",
							                sym_size, " != ", sizeof(Elf_Sym));
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
	
			char const *name = (char const *)(sym->st_name + str_tab);
			if (!is_wanted(name))
				continue;

			sym_tab[out_cnt] = *sym;
			/* set absolute value */
			sym_tab[out_cnt].st_value += map->addr;
			if (verbose)
				Genode::log("Read symbol ", name, " "
				            "val: ", Genode::Hex(sym_tab[out_cnt].st_value));
			out_cnt++;
		}
	}

	void rump_load(rump_symload_fn symload)
	{
		symload(sym_tab, sizeof(Elf_Sym) * out_cnt, (char *)str_tab, str_size);
	}
};


/**
 * Call init functions of libraries
 */
static void _dl_init(Genode::Shared_object::Link_map const *map,
                     rump_modinit_fn mod_init,
                     rump_compload_fn comp_init)
{
	using namespace Genode;
	Shared_object *obj = nullptr;
	try {
		obj = new (heap()) Shared_object(::env(), heap(), map->path,
		                                 Shared_object::BIND_LAZY,
		                                 Shared_object::DONT_KEEP);
	}
	catch (...) { error("could not dlopen ", map->path); return; }

	struct modinfo **mi_start, **mi_end;
	struct rump_component **rc_start, **rc_end;

	mi_start = obj->lookup<modinfo **>("__start_link_set_modules");
	mi_end   = obj->lookup<modinfo **>("__stop_link_set_modules");
	if (verbose)
		log("MI: start: ", mi_start, " end: ", mi_end);
	if (mi_start && mi_end)
		mod_init(mi_start, (Genode::size_t)(mi_end-mi_start));

	rc_start = obj->lookup<rump_component **>("__start_link_set_rump_components");
	rc_end   = obj->lookup<rump_component **>("__stop_link_set_rump_components");
	if (verbose)
		log("RC: start: ", rc_start, " end: ", rc_end);
	if (rc_start && rc_end) {
		for (; rc_start < rc_end; rc_start++)
			comp_init(*rc_start);
	}
}


void rumpuser_dl_bootstrap(rump_modinit_fn domodinit, rump_symload_fn symload,
                           rump_compload_fn compload)
{
	/* open main program and request link map */
	using namespace Genode;

	try {
		obj_main = new (heap()) Shared_object(::env(), heap(), nullptr,
		                                      Shared_object::BIND_NOW,
		                                      Shared_object::KEEP);
	}
	catch (...) { error("could not dlopen the main executable"); return; }

	Shared_object::Link_map const *map = &obj_main->link_map();
	for (; map->next; map = map->next) ;

	Shared_object::Link_map const *curr_map;

	for (curr_map = map; curr_map; curr_map = curr_map->prev) {
		if (!Genode::strcmp(curr_map->path, "rump", 4)) {
			Sym_tab tab(curr_map);
			/* load into rum kernel */
			tab.rump_load(symload);
			/* init modules and components */
			_dl_init(curr_map, domodinit, compload);
		}
	}
	log("BOOTSTRAP");
}


void * rumpuser_dl_globalsym(const char *symname)
{

	void *addr = 0;
	try { addr = obj_main->lookup(symname); }
	catch (...) { }

	if (verbose)
		Genode::log("Lookup: ", symname, " addr ", addr);

	return addr;
}
