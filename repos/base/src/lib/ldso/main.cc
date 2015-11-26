/**
 * \brief  Genode's dynamic linker
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <os/config.h>
#include <util/list.h>
#include <util/string.h>

#include <dynamic.h>
#include <init.h>

using namespace Linker;

namespace Linker {
	struct Dynamic;
	struct Ld;
	struct Ld_vtable;
	struct Binary;
	struct Link_map;
	struct Debug;

};

/**
 * Genode args to the 'main' function
 */
extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;

static    Binary *binary = 0;
bool      Linker::bind_now = false;
Link_map *Link_map::first;

/**
 * Registers dtors
 */
int genode_atexit(Linker::Func);


/**************************************************************
 ** ELF object types (shared object, dynamic binaries, ldso  **
 **************************************************************/

/**
 * The actual ELF object, one per file
 */
struct Linker::Elf_object : Object, Genode::Fifo<Elf_object>::Element
{
	Dynamic  dyn;
	Link_map map;
	unsigned ref_count = 1;
	unsigned flags     = 0;
	bool     relocated = false;

	Elf_object(Dependency const *dep, Elf::Addr reloc_base)
	: Object(reloc_base), dyn(dep)
	{ }

	Elf_object(char const *path, Dependency const *dep, unsigned flags = 0)
	:
	  Object(path, Linker::load(Linker::file(path))), dyn(dep, this, &_file->phdr),
	  flags(flags)
	{
		/* register for static construction and relocation */
		Init::list()->insert(this);
		obj_list()->enqueue(this);

		/* add to link map */
		Debug::state_change(Debug::ADD, nullptr);
		setup_link_map();
		Debug::state_change(Debug::CONSISTENT, &map);
	}

	virtual ~Elf_object()
	{
		if (!_file)
			return;

		if (verbose_loading)
			PDBG("destroy: %s", name());

		/* remove from link map */
		Debug::state_change(Debug::DELETE, &map);
		Link_map::remove(&map);
		Debug::state_change(Debug::CONSISTENT, nullptr);

		/* remove from loaded objects list */
		obj_list()->remove(this);
	}

	/**
	 * Return symbol of given number from ELF
	 */
	Elf::Sym const *symbol(unsigned sym_index) const
	{
		if (sym_index > dyn.hash_table->nchains())
			return 0;

		return dyn.symtab + sym_index;
	}

	/**
	 * Return name of given symbol
	 */
	char const *symbol_name(Elf::Sym const *sym) const {
		return dyn.strtab + sym->st_name;
	}

	/**
	 * Lookup symbol name in this ELF
	 */
	Elf::Sym const *lookup_symbol(char const *name, unsigned long hash) const
	{
		Hash_table *h = dyn.hash_table;

		if (!h->buckets())
			return nullptr;

		unsigned long sym_index = h->buckets()[hash % h->nbuckets()];

		/* traverse hash chain */
		for (; sym_index != STN_UNDEF; sym_index = h->chains()[sym_index])
		{
			/* bad object */
			if (sym_index > h->nchains())
				return nullptr;

			Elf::Sym const *sym      = symbol(sym_index);
			char const     *sym_name = symbol_name(sym);

			/* this omitts everything but 'NOTYPE', 'OBJECT', and 'FUNC' */
			if (sym->type() > STT_FUNC)
				continue;

			if (sym->st_value == 0)
				continue;

			/* check for symbol name */
			if (name[0] != sym_name[0] || Genode::strcmp(name, sym_name))
				continue;

			return sym;
		}

		return nullptr;
	}

	/**
	 * Fill-out link map infos for this ELF
	 */
	void setup_link_map()
	{
		map.addr    = _file ? _file->start + reloc_base() : reloc_base();
		map.path    = name();
		map.dynamic = (void *)dyn.dynamic;

		Link_map::add(&map);
	};


	Link_map *link_map() override { return &map; }
	Dynamic  *dynamic()  override { return &dyn; }

	void relocate() override
	{
		if (!relocated)
			dyn.relocate();

		relocated = true;
	}

	void info(Genode::addr_t addr, Genode::Address_info &info) override
	{
		info.path = name();
		info.base = map.addr;
		info.addr = 0;

		Hash_table *h = dyn.hash_table;

		for (unsigned long sym_index = 0; sym_index < h->nchains(); sym_index++)
		{
			Elf::Sym const *sym = symbol(sym_index);

			/* skip */
			if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_COMMON)
				continue;

			Genode::addr_t sym_addr = reloc_base() + sym->st_value;
			if (sym_addr > addr || sym_addr < info.addr)
				continue;

			info.addr =  sym_addr;
			info.name =  symbol_name(sym);

			if (info.addr == addr)
				break;
		}

		if (!info.addr)
			throw Genode::Address_info::Invalid_address();
	}

	/**
	 * Next in initializion list
	 */
	Object *next_init() const override {
		return Genode::List<Object>::Element::next();
	}

	/**
	 * Next in object list
	 */
	Object *next_obj() const override {
		return Genode::Fifo<Elf_object>::Element::next();
	}

	/**
	 * Object list
	 */
	static Genode::Fifo<Elf_object> *obj_list()
	{
		static Genode::Fifo<Elf_object> _list;
		return &_list;
	}


	void load()   override { ref_count++; }
	bool unload() override { return !(--ref_count) && !(flags & Genode::Shared_object::KEEP); }

	bool is_linker() const override { return false; }
	bool is_binary() const override { return false; }
};


/**
 * The dynamic linker object (ld.lib.so)
 */
struct Linker::Ld : Dependency, Elf_object
{
	Ld()
	: Dependency(this, nullptr), Elf_object(this, relocation_address())
	{
		Genode::strncpy(_name, linker_name(), Object::MAX_PATH);
	}

	void setup_link_map()
	{
		Elf_object::setup_link_map();

		/**
		 * Use DT_HASH table address for linker, assuming that it will always be at
		 * the beginning of the file
		 */
		map.addr = trunc_page((Elf::Addr)dynamic()->hash_table);
	}

	void load_phdr()
	{
		_file = Linker::load(name(), false);
	}

	void relocate_global() { dynamic()->relocate_non_plt(true); }
	void update_dependency(Dependency const *dep) { dynamic()->dep = dep; }

	static Ld *linker();

	bool is_linker() const override { return true; }

	/**
	 * Entry point for jump relocations, it is called from assembly code and is implemented
	 * right below)
	 */
	static Elf::Addr jmp_slot(Dependency const *dep, Elf::Size index) asm("jmp_slot");
};


Elf::Addr Ld::jmp_slot(Dependency const *dep, Elf::Size index)
{
	Genode::Lock::Guard guard(Elf_object::lock());

	if (verbose_relocation)
		PDBG("SLOT %p " EFMT, dep->obj, index);

	try {
		Reloc_jmpslot slot(dep, dep->obj->dynamic()->pltrel_type, 
		                   dep->obj->dynamic()->pltrel, index);
		return slot.target_addr();
	} catch (...) { PERR("LD: Jump slot relocation failed. FATAL!"); }

	return 0;
}


/**
 * Ld object with different vtable typeinfo
 */
struct Linker::Ld_vtable : Ld
{
	Ld_vtable()
	{
		Elf_object::obj_list()->enqueue(this);
	}
};

/**
 * Linker object used during bootstrapping on stack (see: 'init_rtld')
 */
Linker::Ld *Linker::Ld::linker()
{
	static Ld_vtable _linker;
	return &_linker;
}


/**
 * The dynamic binary to load
 */
struct Linker::Binary : Root_object, Elf_object
{
	Binary()
	: Elf_object(binary_name(), new (Genode::env()->heap()) Dependency(this, this))
	{
		/* create dep for binary and linker */
		Dependency *binary = const_cast<Dependency *>(dynamic()->dep);
		dep.enqueue(binary);
		Dependency *linker = new (Genode::env()->heap()) Dependency(Ld::linker(), this);
		dep.enqueue(linker);

		/* update linker dep */
		Ld::linker()->update_dependency(linker);

		/* place linker on second place in link map as well */
		Ld::linker()->setup_link_map();

		/* load dependencies */
		binary->load_needed(&dep);

		/* relocate and call constructors */
		Init::list()->initialize();
	}

	Elf::Addr lookup_symbol(char const *name)
	{
		Elf::Sym const *symbol = 0;

		if ((symbol = Elf_object::lookup_symbol(name, Hash_table::hash(name))))
			return reloc_base() + symbol->st_value;

		return 0;
	}

	int call_entry_point()
	{
		/* call static construtors and register destructors */
		Func * const ctors_start = (Func *)lookup_symbol("_ctors_start");
		Func * const ctors_end   = (Func *)lookup_symbol("_ctors_end");
		for (Func * ctor = ctors_end; ctor != ctors_start; (*--ctor)());

		Func * const dtors_start = (Func *)lookup_symbol("_dtors_start");
		Func * const dtors_end   = (Func *)lookup_symbol("_dtors_end");
		for (Func * dtor = dtors_start; dtor != dtors_end; genode_atexit(*dtor++));

		/* call main function of the program */
		typedef int (*Main)(int, char **, char **);
		Main const main = reinterpret_cast<Main>(_file->entry);

		return main(genode_argc, genode_argv, genode_envp);
	}

	void relocate() override
	{
		/* relocate ourselves */
		Elf_object::relocate();

		/*
		 * After having loaded the main program, we relocate the linker's
		 * symbols again such that, for example type informations, which are
		 * also present within the main program, become relocated to the correct
		 * positions
		 */
		Ld::linker()->relocate_global();
	}

	bool is_binary() const override { return true; }
};


/***************************************
 ** Global Linker namespace functions **
 ***************************************/

Object *Linker::load(char const *path, Dependency *dep, unsigned flags)
{
	for (Object *e = Elf_object::obj_list()->head(); e; e = e->next_obj()) {

		if (verbose_loading)
			PDBG("LOAD: %s == %s", Linker::file(path), e->name());

		if (!Genode::strcmp(Linker::file(path), e->name())) {
			e->load();
			return e;
		}
	}

	Elf_object *e = new (Genode::env()->heap()) Elf_object(path, dep, flags);
	dep->obj = e;
	return e;
}


Object *Linker::obj_list_head()
{
	return Elf_object::obj_list()->head();
}


Elf::Sym const *Linker::lookup_symbol(unsigned sym_index, Dependency const *dep,
                                      Elf::Addr *base, bool undef, bool other)
{
	Elf_object const *e      = static_cast<Elf_object *>(dep->obj);
	Elf::Sym   const *symbol = e->symbol(sym_index);

	if (!symbol) {
		PWRN("LD: Unkown symbol index %x", sym_index);
		return 0;
	}

	if (symbol->bind() == STB_LOCAL) {
		*base = dep->obj->reloc_base();
		return symbol;
	}

	return lookup_symbol(e->symbol_name(symbol), dep, base, undef, other);
}


Elf::Sym const *Linker::lookup_symbol(char const *name, Dependency const *dep,
                                      Elf::Addr *base, bool undef, bool other)
{
	Dependency const *curr        = dep->root ? dep->root->dep.head() : dep;
	unsigned long     hash        = Hash_table::hash(name);
	Elf::Sym   const *weak_symbol = 0;
	Elf::Addr        weak_base    = 0;
	Elf::Sym   const *symbol      = 0;

	//TODO: handle vertab and search in object list
	for (;curr; curr = curr->next()) {

		if (other && curr == dep)
			continue;

		Elf_object const *elf = static_cast<Elf_object *>(curr->obj);

		if ((symbol = elf->lookup_symbol(name, hash)) && (symbol->st_value || undef)) {

			if (dep->root && verbose_lookup)
				PINF("Lookup %s obj_src %s st %p info %x weak: %u", name, elf->name(), symbol, symbol->st_info, symbol->weak());

			if (!undef && symbol->st_shndx == SHN_UNDEF)
				continue;

			if (!symbol->weak() && symbol->st_shndx != SHN_UNDEF) {
				*base = elf->reloc_base();
				return symbol;
			}

			if (!weak_symbol) {
				weak_symbol = symbol;
				weak_base   = elf->reloc_base();
			}
		}
	}

	/* try searching binary's dependencies */
	if (!weak_symbol && dep->root && dep != binary->dep.head())
		return lookup_symbol(name, binary->dep.head(), base, undef, other);

	if (dep->root && verbose_lookup)
		PDBG("Return %p", weak_symbol);

	if (!weak_symbol)
		throw Not_found();

	*base = weak_base;
	return weak_symbol;
}


void Linker::load_linker_phdr()
{
	if (!Ld::linker()->file())
		Ld::linker()->load_phdr();
}


/********************
 ** Initialization **
 ********************/

/**
 * Called before anything else, even '_main', we cannot access any global data
 * here, we have to relocate our own ELF first
 */
extern "C" void init_rtld()
{
	/*
	 * Allocate on stack, since the linker has not been relocated yet, the vtable
	 * type relocation might prdouce a wrong vtable pointer (at least on ARM), do
	 * not call any virtual funtions of this object
	 */
	Ld linker_stack;
	linker_stack.relocate();

	/* make sure this does not get destroyed the usual way */
	linker_stack.ref_count++;

	/*
	 * Create actual linker object with different vtable type and set PLT to new
	 * DAG.
	 */
	Ld::linker()->dynamic()->plt_setup();
}


static void dump_loaded()
{
	Object *o = Elf_object::obj_list()->head();
	for(; o; o = o->next_obj()) {

		if (o->is_binary())
			continue;

		Genode::printf("  " EFMT " .. " EFMT ": %s\n",
		                o->link_map()->addr, o->link_map()->addr + o->size() - 1,
		                o->name());
	}
}


int main()
{
	/* load program headers of linker now */
	if (!Ld::linker()->file())
		Ld::linker()->load_phdr();

	/* read configuration */
	try {
		/* bind immediately */
		bind_now = Genode::config()->xml_node().attribute("ld_bind_now").has_value("yes");
	} catch (...) { }

	/* load binary and all dependencies */
	try {
		binary = new(Genode::env()->heap()) Binary();
	} catch (...) {
			PERR("LD: Failed to load program");
			return -1;
	}

	/* print loaded object information */
	try {
		if (Genode::config()->xml_node().attribute("ld_verbose").has_value("yes")) {
			PINF("  %lx .. %lx: context area", Genode::Native_config::context_area_virtual_base(),
			     Genode::Native_config::context_area_virtual_base() +
			     Genode::Native_config::context_area_virtual_size() - 1);
			dump_loaded();
		}
	} catch (...) {  }

	Link_map::dump();

	/* start binary */
	return binary->call_entry_point();
}

