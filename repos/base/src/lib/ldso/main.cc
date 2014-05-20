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
#include <base/shared_object.h>
#include <os/config.h>
#include <util/list.h>
#include <util/string.h>

#include <relocation.h>

using namespace Linker;


/**
 * Offset of dynamic section of this ELF is filled out during linkage
 */
extern Genode::addr_t _DYNAMIC;

/**
 * Genode args to the 'main' function
 */
extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;


namespace Linker {
	struct Dynamic;
	struct Hash_table;
	struct Ld;
	struct Ld_vtable;
	struct Binary;
	struct Link_map;
	struct Debug;

	typedef void (*Func)(void);
};

static bool    bind_now  = false;
static Binary *binary    = 0;

/**
 * Registers dtors
 */
int genode_atexit(Linker::Func);

/**
 * LIBC debug support
 */
extern "C" void brk(Debug *, Link_map *) { }

struct Linker::Debug
{
	/*
	 * This state value describes the mapping change taking place when
	 * the brk address is called.
	 */
	enum State {
		CONSISTENT, /* mapping change is complete */
		ADD,        /* beginning to add a new object */
		DELETE      /* beginning to remove an object mapping */
	};

	Debug() : Brk(brk) { }

	int             version = 1;        /* unused */
	struct Link_map *map    = nullptr;; /* start of link map */

	/*
	 * This is the address of a function internal to the run-time linker, that
	 * will always be called when the linker begins to map in a library or unmap
	 * it, and again when the mapping change is complete.  The debugger can set a
	 * breakpoint at this address if it wants to notice shared object mapping
	 * changes.
	 */
	void  (*Brk)(Debug *, Link_map *);
	State state = CONSISTENT;

	static void state_change(State s, Link_map *m)
	{
		d()->state = s;
		d()->Brk(d(), m);
	}

	static Debug *d()
	{
		static Debug _d;
		return &_d;
	}
};


/**
 * Link map
 */
struct Linker::Link_map
{
	Elf::Addr   addr;    /* base address of library */
	char const *path;    /* path */
	void const *dynamic; /* DYNAMIC section */

	Link_map   *next = nullptr;
	Link_map   *prev = nullptr;

	static Link_map *first;

	static void add(Link_map *map)
	{
		map->next = nullptr;;
		if (!first) {
			first           = map;
			Debug::d()->map = map;
			return;
		}

		Link_map *m;
		for (m = first; m->next; m = m->next) ;

		m->next   = map;
		map->prev = m;
	}

	static void remove(Link_map *map)
	{
			if (map->prev)
				map->prev->next = map->next;

			if (map->next)
				map->next->prev = map->prev;

			if (map == first)
				first = map->next;
	}

	static void dump()
	{
		if (!verbose_link_map)
			return;

		for (Link_map *m = first; m; m = m->next)
			PINF("MAP: addr: " EFMT " dynamic: %p %s m: %p p: %p n: %p",
			     m->addr, m->dynamic, m->path, m, m->prev, m->next);
	}
};

Link_map *Link_map::first;


/**
 * ELF hash table and hash function
 */
struct Linker::Hash_table
{
	unsigned long nbuckets() const { return *((Elf::Hashelt *)this); }
	unsigned long nchains()  const { return *(((Elf::Hashelt *)this) + 1); }

	Elf::Hashelt const *buckets() { return ((Elf::Hashelt *)this + 2); }
	Elf::Hashelt const *chains()  { return buckets() + nbuckets(); }

	/**
	 * ELF hash function Figure 5.12 of the 'System V ABI'
	 */
	static unsigned long hash(char const *name)
	{
		unsigned const char  *p = (unsigned char const *)name;
		unsigned long         h = 0, g;

		while (*p) {
			h = (h << 4) + *p++;
			if ((g = h & 0xf0000000) != 0)
				h ^= g >> 24;
			h &= ~g;
		}
		return h;
	}
};


/**
 * .dynamic section entries
 */
struct Linker::Dynamic
{
	struct Needed : Genode::Fifo<Needed>::Element
	{
		Genode::off_t offset;

		Needed(Genode::off_t offset) : offset(offset) { }

		char const *path(char const *strtab)
		{
			return ((char const *)(strtab + offset));
		}

		char const *name(char const *strtab)
		{
			return file(path(strtab));
		}
	};

	Dag       const     *dag;
	Object    const     *obj;
	Elf::Dyn  const     *dynamic;

	Hash_table          *hash_table    = nullptr;

	Elf::Rela           *reloca        = nullptr;
	unsigned long        reloca_size   = 0;

	Elf::Sym            *symtab        = nullptr;
	char                *strtab        = nullptr;
	unsigned long        strtab_size   = 0;

	Elf::Addr           *pltgot        = nullptr;

	Elf::Rel            *pltrel        = nullptr;
	unsigned long        pltrel_size   = 0;
	D_tag                pltrel_type   = DT_NULL;

	Func                 init_function = nullptr;

	Elf::Rel            *rel           = nullptr;
	unsigned long        rel_size      = 0;

	Genode::Fifo<Needed> needed;

	Dynamic(Dag const *dag)
	:
	  dag(dag), obj(dag->obj), dynamic((Elf::Dyn *)(obj->reloc_base() + &_DYNAMIC))
	{
		init();
	}

	Dynamic(Dag const *dag, Object const *obj, Linker::Phdr const *phdr)
	:
		dag(dag), obj(obj), dynamic(find_dynamic(phdr))
	{
		init();
	}

	~Dynamic()
	{
		Needed *n;
		while ((n = needed.dequeue()))
			destroy(Genode::env()->heap(), n);
	}

	Elf::Dyn const *find_dynamic(Linker::Phdr const *p)
	{
		for (unsigned i = 0; i < p->count; i++)
			if (p->phdr[i].p_type == PT_DYNAMIC)
				return reinterpret_cast<Elf::Dyn const *>(p->phdr[i].p_vaddr + obj->reloc_base());
	
		return 0;
	}

	void section_dt_needed(Elf::Dyn const *d)
	{
		Needed *n = new(Genode::env()->heap()) Needed(d->un.ptr);
		needed.enqueue(n);
	}

	template <typename T>
	void section(T *member, Elf::Dyn const *d)
	{
		*member = (T)(obj->reloc_base() + d->un.ptr);
	}

	void section_dt_debug(Elf::Dyn const *d)
	{
		Elf::Dyn *_d = const_cast<Elf::Dyn *>(d);
		_d->un.ptr   = (Elf::Addr)Debug::d();
	}

	void init()
	{
		for (Elf::Dyn const *d = dynamic; d->tag != DT_NULL; d++) {
			switch (d->tag) {

				case DT_NEEDED  : section_dt_needed(d);                              break;
				case DT_PLTRELSZ: pltrel_size = d->un.val;                           break;
				case DT_PLTGOT  : section<typeof(pltgot)>(&pltgot, d);               break;
				case DT_HASH    : section<typeof(hash_table)>(&hash_table, d);       break;
				case DT_RELA    : section<typeof(reloca)>(&reloca, d);               break;
				case DT_RELASZ  : reloca_size = d->un.val;                           break;
				case DT_SYMTAB  : section<typeof(symtab)>(&symtab, d);               break;
				case DT_STRTAB  : section<typeof(strtab)>(&strtab, d);               break;
				case DT_STRSZ   : strtab_size = d->un.val;                           break;
				case DT_INIT    : section<typeof(init_function)>(&init_function, d); break;
				case DT_PLTREL  : pltrel_type = (D_tag)d->un.val;                    break;
				case DT_JMPREL  : section<typeof(pltrel)>(&pltrel, d);               break;
				case DT_REL     : section<typeof(rel)>(&rel, d);                     break;
				case DT_RELSZ   : rel_size = d->un.val;                              break;
				case DT_DEBUG   : section_dt_debug(d);                               break;
				default:
					trace("DT", d->tag, 0, 0);
					break;
			}
		}
	}

	void relocate()
	{
		plt_setup();

		if (pltrel_size) {
			switch (pltrel_type) {

				case DT_RELA:
				case DT_REL:
					Reloc_plt(obj, pltrel_type, pltrel, pltrel_size);
					break;
				default:
					PERR("LD: Invalid PLT relocation %u", pltrel_type);
					throw Incompatible();
			}
		}

		relocate_non_plt();
	}

	void plt_setup()
	{
		if (pltgot)
			Plt_got r(dag, pltgot);
	}

	void relocate_non_plt(bool second_pass = false)
	{
		if (reloca)
			Reloc_non_plt r(dag, reloca, reloca_size);

		if (rel)
			Reloc_non_plt r(dag, rel, rel_size, second_pass);

		if (bind_now)
			Reloc_bind_now r(dag, pltrel, pltrel_size);
	}
};


static void register_initializer(Elf_object *elf);

/**
 * The actual ELF object, one per file
 */
struct Linker::Elf_object : Object, Genode::List<Elf_object>::Element,
                            Genode::Fifo<Elf_object>::Element
{
	Dynamic  dynamic;
	Link_map map;
	unsigned ref_count = 1;
	unsigned flags     = 0;

	Elf_object(Dag const *dag) : dynamic(dag)
	{ }

	Elf_object(char const *path, Dag const *dag, unsigned flags = 0)
	:
	  Object(path, Linker::load(Linker::file(path))), dynamic(dag, this, &_file->phdr),
	  flags(flags)
	{
		/* register for static construction and relocation */
		register_initializer(this);
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

	void setup_link_map()
	{
		map.addr    = _file ? _file->start + reloc_base() : reloc_base();
		map.path    = name();
		map.dynamic = (void *)dynamic.dynamic;

		Link_map::add(&map);
	};

	virtual void relocate() { dynamic.relocate(); }

	/**
	 * Return symbol of given number from ELF
	 */
	Elf::Sym const * symbol(unsigned sym_index) const
	{
		if (sym_index > dynamic.hash_table->nchains())
			return 0;

		return dynamic.symtab + sym_index;
	}

	/**
	 * Return name of given symbol
	 */
	char const *symbol_name(Elf::Sym const *sym) const
	{
		return dynamic.strtab + sym->st_name;
	}

	/**
	 * Lookup symbol name in this ELF
	 */
	Elf::Sym const * lookup_symbol(char const *name, unsigned long hash) const
	{
		Hash_table *h = dynamic.hash_table;

		if (!h->buckets())
			return 0;

		unsigned long sym_index = h->buckets()[hash % h->nbuckets()];

		/* traverse hash chain */
		for (; sym_index != STN_UNDEF; sym_index = h->chains()[sym_index])
		{
			/* bad object */
			if (sym_index > h->nchains())
				return 0;

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

		return 0;
	}

	/**
	 * Search for symbol at addr
	 */
	void info(Genode::addr_t addr, Genode::Address_info &info) const
	{
		info.path = name();
		info.base = map.addr;
		info.addr = 0;

		Hash_table *h = dynamic.hash_table;

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

	Elf_object *init_next() const
	{
		return Genode::List<Elf_object>::Element::next();
	}


	Elf_object *obj_next() const
	{
		return Genode::Fifo<Elf_object>::Element::next();
	}

	static Genode::Fifo<Elf_object> *obj_list()
	{
		static Genode::Fifo<Elf_object> _list;
		return &_list;
	}

	static Elf_object *find_obj(Genode::addr_t addr)
	{
		for (Elf_object *e = obj_list()->head(); e; e = e->obj_next())
			if (addr >= e->map.addr  && addr < e->map.addr + e->size())
				return e;

		throw Genode::Address_info::Invalid_address();
	}

	static Elf_object *load(char const *path, Dag *dag, unsigned flags = 0)
	{
		for (Elf_object *e = obj_list()->head(); e; e = e->obj_next()) {

			if (verbose_loading)
				PDBG("LOAD: %s == %s", Linker::file(path), e->name());

			if (!Genode::strcmp(Linker::file(path), e->name())) {
				e->ref_count++;
				return e;
			}
		}

		Elf_object *e = new (Genode::env()->heap()) Elf_object(path, dag, flags);
		dag->obj = e;
		return e;
	}

	bool unload() { return !(--ref_count) && !(flags & Genode::Shared_object::KEEP); }

	static Genode::Lock & lock()
	{
		static Genode::Lock _lock;
		return _lock;
	}


	Elf::Phdr const *phdr_exidx() const
	{
		for (unsigned i = 0; i < _file->elf_phdr_count(); i++) {
			Elf::Phdr const *ph = _file->elf_phdr(i);

			if (ph->p_type == PT_ARM_EXIDX)
				return ph;
		}

		return 0;
	}

	bool is_linker() const override { return false; }
	bool is_binary() const override { return false; }
};


/**
 * Handle static construction and relocation of ELF files
 */
struct Init : Genode::List<Elf_object>
{
	static Init *list()
	{
		static Init _list;
		return &_list;
	}

	Elf_object *contains(char const *file)
	{
		for (Elf_object *e = first(); e; e = e->init_next())
			if (!Genode::strcmp(file, e->name()))
				return e;

		return nullptr;
	}

	void reorder(Elf_object const *elf)
	{
		/* put in front of initializer list */
		remove(elf);
		insert(elf);

		/* re-order dependencies */
		for (Dynamic::Needed *n = elf->dynamic.needed.head(); n; n = n->next()) {
			char const *path = n->path(elf->dynamic.strtab);
			Elf_object *e;

			if ((e = contains(Linker::file(path))))
				reorder(e);
		}
	}

	void initialize()
	{
		Elf_object *obj = first();

		/* relocate */
		for (; obj; obj = obj->init_next()) {
			if (verbose_relocation)
				PDBG("Relocate %s", obj->name());
			obj->relocate();
		}

		/* call static constructors */
		obj = first();
		while (obj) {

			if (obj->dynamic.init_function) {

				if (verbose_relocation)
					PDBG("%s init func %p", obj->name(), obj->dynamic.init_function);

				obj->dynamic.init_function();
			}

			Elf_object *next = obj->init_next();
			remove(obj);
			obj = next;
		}
	}
};


void register_initializer(Elf_object *elf)
{
	Init::list()->insert(elf);
}


/**
 * Dag node
 */
Linker::Dag::Dag(char const *path, Root_object *root, Genode::Fifo<Dag> * const dag, unsigned flags)
	: obj(Elf_object::load(path, this, flags)), root(root)
{
	dag->enqueue(this);
	load_needed(dag, flags);
}


Linker::Dag::~Dag()
{
	if ((static_cast<Elf_object *>(obj))->unload()) {

		if (verbose_loading)
			PDBG("Destroy: %s\n", obj->name());

		destroy(Genode::env()->heap(), obj);
	}
}


bool Linker::Dag::in_dag(char const *file, Genode::Fifo<Dag> * const dag)
{
	for (Dag *d = dag->head(); d; d = d->next())
		if (!Genode::strcmp(file, d->obj->name()))
			return true;

	return false;
}


void Linker::Dag::load_needed(Genode::Fifo<Dag> * const dag, unsigned flags)
{
	Elf_object *elf = static_cast<Elf_object *>(obj);

	for (Dynamic::Needed *n = elf->dynamic.needed.head(); n; n = n->next()) {
		char const *path = n->path(elf->dynamic.strtab);
		Elf_object *e;

		if (!in_dag(Linker::file(path), dag))
			new (Genode::env()->heap()) Dag(path, root, dag, flags);

		/* re-order initializer list, if needed object has been already added */
		else if ((e = Init::list()->contains(Linker::file(path))))
			Init::list()->reorder(e);
	}
}


/**
 * The dynamic linker has its own ELF ojbect type
 */
struct Linker::Ld : Dag, Elf_object
{
	Ld() : Dag(this, nullptr), Elf_object(this)
	{
		Genode::strncpy(_name, linker_name(), Object::MAX_PATH);

		trace("LD", 0, 0, 0);
	}

	void setup_link_map()
	{
		Elf_object::setup_link_map();

		/**
		 * Use DT_HASH table address for linker, assuming that it will always be at
		 * the beginning of the file
		 */
		map.addr = trunc_page((Elf::Addr)dynamic.hash_table);
	}

	void load_phdr()
	{
		_file = Linker::load(name(), false);
	}

	void relocate_global() { dynamic.relocate_non_plt(true); }
	void update_dag(Dag const *dag) { dynamic.dag = dag; }

	static Ld *linker();

	bool is_linker() const override { return true; }

	/**
	 * Entry point for jump relocations, it is called from assembly
	 */
	static Elf::Addr jmp_slot(Dag const *dag, Elf::Size offset) asm("jmp_slot");
};


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


Linker::Ld *Linker::Ld::linker()
{
	static Ld_vtable _linker;
	return &_linker;
}


Elf::Addr Ld::jmp_slot(Dag const *dag, Elf::Size index)
{
	Elf_object const *obj = static_cast<Elf_object *>(dag->obj);

	Genode::Lock::Guard guard(Elf_object::lock());

	if (verbose_relocation)
		PDBG("SLOT %p " EFMT, obj, index);

	try {
		Reloc_jmpslot slot(dag, obj->dynamic.pltrel_type, obj->dynamic.pltrel, index);
		return slot.target_addr();
	} catch (...) { PERR("LD: Jump slot relocation failed. FATAL!"); }

	return 0;
}


struct Linker::Root_object
{
	Genode::Fifo<Dag> dag;

	/* main root */
	Root_object() { };

	/* runtime loaded root components */
	Root_object(char const *path, unsigned flags = 0)
	{
		new (Genode::env()->heap()) Dag(path, this, &dag, flags);

		/* provide Genode base library access */
		new (Genode::env()->heap()) Dag(linker_name(), this, &dag);;

		/* relocate and call constructors */
		Init::list()->initialize();
	}

	~Root_object()
	{
		Dag *d;
		while ((d = dag.dequeue()))
			destroy(Genode::env()->heap(), d);
	}

	Link_map const *link_map() const
	{
		Elf_object const *obj = static_cast<Elf_object *>(dag.head()->obj);
		return obj ? &obj->map : 0;
	}
};


/**
 * The program to load has its own ELF object type
 */
struct Linker::Binary : Root_object, Elf_object
{
	Binary()
	: Elf_object(binary_name(), new (Genode::env()->heap()) Dag(this, this))
	{
		/* create dag for binary and linker */
		Dag *binary = const_cast<Dag *>(dynamic.dag);
		dag.enqueue(binary);
		Dag *linker = new (Genode::env()->heap()) Dag(Ld::linker(), this);
		dag.enqueue(linker);

		/* update linker dag */
		Ld::linker()->update_dag(linker);

		/* place linker on second place in link map as well */
		Ld::linker()->setup_link_map();

		/* load dependencies */
		binary->load_needed(&dag);

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


/**
 * This is called mostly from the relocation code
 * XXX: 'undef' needs to be true for dlsym
 */
Elf::Sym const *Linker::locate_symbol(unsigned sym_index, Dag const *dag, Elf::Addr *base, bool undef, bool other)
{
	Elf_object const *e      = static_cast<Elf_object *>(dag->obj);
	Elf::Sym   const *symbol = e->symbol(sym_index);

	if (!symbol) {
		PWRN("LD: Unkown symbol index %x", sym_index);
		return 0;
	}

	if (symbol->bind() == STB_LOCAL) {
		trace("NONLOCAL", symbol->bind(), 0, 0);
		*base = dag->obj->reloc_base();
		return symbol;
	}

	return search_symbol(e->symbol_name(symbol), dag, base, undef, other);
}


Elf::Sym const *Linker::search_symbol(char const *name, Dag const *dag, Elf::Addr *base, bool undef, bool other)
{
	Dag const        *curr        = dag->root ? dag->root->dag.head() : dag;
	unsigned long     hash        = Hash_table::hash(name);
	Elf::Sym   const *weak_symbol = 0;
	Elf::Addr        weak_base    = 0;
	Elf::Sym   const *symbol      = 0;

	//TODO: handle vertab and search in object list
	for (;curr; curr = curr->next()) {

		if (other && curr == dag)
			continue;

		Elf_object const *elf = static_cast<Elf_object *>(curr->obj);

		if ((symbol = elf->lookup_symbol(name, hash)) && (symbol->st_value || undef)) {

			if (dag->root && verbose_lookup)
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

	/* try searching binary's DAG */
	if (!weak_symbol && dag->root && dag != binary->dag.head())
		return search_symbol(name, binary->dag.head(), base, undef, other);

	if (dag->root && verbose_lookup)
		PDBG("Return %p", weak_symbol);

	if (!weak_symbol)
		throw Not_found();

	*base = weak_base;
	return weak_symbol;
}


/**
 * Called before anything else, even '_main', we cannot access any global data
 * here, we have to relocate our own ELF first
 */
extern "C" void init_rtld()
{
	trace("init_rtld", 0, 0, 0);

	/*
	 * Allocate on stack, since the linker has not been relocated yet, the vtable
	 * type relocation might prdouce a wrong vtable pointer (at least on ARM), do
	 * not call any virtual funtions of this object
	 */
	Ld linker_stack;
	linker_stack.relocate();

	/* make sure this does not get destroyed the usual way */
	linker_stack.ref_count++;

	trace("init_rtld finished", 0, 0, 0);

	/*
	 * Create actual linker object with different vtable type  and set PLT to new
	 * DAG.
	 */
	Ld::linker()->dynamic.plt_setup();
}


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


extern "C" int dl_iterate_phdr(int (*callback) (Phdr_info *info, Genode::size_t size, void *data), void *data)
{
	int err = 0;
	Phdr_info info;

	Genode::Lock::Guard guard(Elf_object::lock());

	Elf_object *e = Elf_object::obj_list()->head();
	for (;e; e = e->obj_next()) {

		info.addr  = e->reloc_base();
		info.name  = e->name();
		info.phdr  = e->file()->phdr.phdr;
		info.phnum = e->file()->phdr.count;

		if (verbose_exception)
			PDBG("%s reloc " EFMT, e->name(), e->reloc_base());

		if ((err = callback(&info, sizeof(Phdr_info), data)))
			break;
	}

	return err;
}


/**
 * Find ELF and exceptions table segment that that is located under 'pc',
 * address of excption table and number of entries 'pcount'
 */
extern "C" unsigned long dl_unwind_find_exidx(unsigned long pc, int *pcount)
{
	/* size of exception table entries */
	enum { EXIDX_ENTRY_SIZE = 8 };
	*pcount = 0;

	/*
	 * Since this function may be called before the main function, load linker's
	 * program header now
	 */
	if (!Ld::linker()->file())
		Ld::linker()->load_phdr();

	Elf_object *e = Elf_object::obj_list()->head();
	for (; e; e = e->obj_next())
	{
		/* address of first PT_LOAD header */
		Genode::addr_t base = e->reloc_base() + e->file()->start;

		/* is the 'pc' somewhere within this ELF image */
		if ((pc < base) || (pc >= base + e->file()->size))
			continue;

		/* retrieve PHDR of exception-table segment */
		Elf::Phdr const *exidx = e->phdr_exidx();
		if (!exidx)
			continue;

		*pcount = exidx->p_memsz / EXIDX_ENTRY_SIZE;
		return exidx->p_vaddr + e->reloc_base();
	}

	return 0;
}


/*****************************
 ** Shared object interface **
 *****************************/

static Root_object *to_root(void *h)
{
	return static_cast<Root_object *>(h);
}


Genode::Shared_object::Shared_object(char const *file, unsigned flags)
{
	using namespace Linker;

	if (verbose_shared)
		PDBG("open '%s'", file ? file : "binary");

	try {
		Genode::Lock::Guard guard(Elf_object::lock());

		/* update bind now variable */
		bind_now = (flags & Shared_object::NOW) ? true : false;

		_handle = (Root_object *)new (Genode::env()->heap()) Root_object(file ? file : binary_name(), flags);
	} catch (...) { throw Invalid_file(); }
}


void *Genode::Shared_object::_lookup(const char *name) const
{
	using namespace Linker;

	if (verbose_shared)
		PDBG("lookup '%s'", name);

	try {
		Genode::Lock::Guard guard(Elf_object::lock());

		Elf::Addr       base;
		Root_object    *root   = to_root(_handle);
		Elf::Sym const *symbol = search_symbol(name, root->dag.head(), &base, true);

		return (void *)(base + symbol->st_value);
	} catch (...) { throw Shared_object::Invalid_symbol(); }
}


Genode::Shared_object::Link_map const * Genode::Shared_object::link_map() const
{
	return (Link_map const *)to_root(_handle)->link_map();
}


Genode::Shared_object::~Shared_object()
{
	using namespace Linker;

	if (verbose_shared)
		PDBG("close");

	Genode::Lock::Guard guard(Elf_object::lock());
	destroy(Genode::env()->heap(), to_root(_handle));
}


Genode::Address_info::Address_info(Genode::addr_t address)
{
	using namespace Genode;

	if (verbose_shared)
		PDBG("request: %lx", address);

	Elf_object *e = Elf_object::find_obj(address);
	e->info(address, *this);

	if (verbose_shared)
		PDBG("Found: obj: %s sym: %s addr: %lx", path, name, addr);
}


static void dump_loaded()
{
	Elf_object *o = Elf_object::obj_list()->head();
	for(; o; o = o->obj_next()) {

		if (o->is_binary())
			continue;

		Elf_object *e = static_cast<Elf_object *>(o);
		Genode::printf("  " EFMT " .. " EFMT ": %s\n",
		                e->map.addr, e->map.addr + e->size() - 1, e->name());
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
		if (Genode::config()->xml_node().attribute("ld_verbose").has_value("yes"))
			dump_loaded();
	} catch (...) {  }

	Link_map::dump();

	/* start binary */
	return binary->call_entry_point();
}

