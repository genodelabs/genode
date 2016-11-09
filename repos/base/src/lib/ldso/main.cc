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

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <util/list.h>
#include <util/string.h>
#include <base/thread.h>
#include <base/heap.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>
#include <base/internal/globals.h>

/* local includes */
#include <dynamic.h>
#include <init.h>
#include <region_map.h>

using namespace Linker;

namespace Linker {
	struct Dynamic;
	struct Ld;
	struct Ld_vtable;
	struct Binary;
	struct Link_map;
	struct Debug;
	struct Config;
};

static    Binary *binary_ptr = nullptr;
bool      Linker::verbose  = false;
Link_map *Link_map::first;

/**
 * Registers dtors
 */
int genode_atexit(Linker::Func);


Linker::Region_map::Constructible_region_map &Linker::Region_map::r()
{
	/*
	 * The capabilities in this class become invalid when doing a
	 * fork in the noux environment. Hence avoid destruction of
	 * the singleton object as the destructor would try to access
	 * the capabilities also in the forked process.
	 */
	return *unmanaged_singleton<Constructible_region_map>();
}


Genode::Lock &Linker::lock()
{
	static Lock _lock;
	return _lock;
}


/**************************************************************
 ** ELF object types (shared object, dynamic binaries, ldso  **
 **************************************************************/

/**
 * The actual ELF object, one per file
 */
class Linker::Elf_object : public Object, public Fifo<Elf_object>::Element
{
	private:

		Link_map       _map;
		unsigned       _ref_count = 1;
		unsigned const _keep      = KEEP;
		bool           _relocated = false;

		/*
		 * Optional ELF file, skipped for initial 'Ld' initialization
		 */
		Lazy_volatile_object<Elf_file> _elf_file;


		bool _object_init(Object::Name const &name, Elf::Addr reloc_base)
		{
			Object::init(name, reloc_base);
			return true;
		}

		bool _init_elf_file(Env &env, Allocator &md_alloc, char const *path)
		{
			_elf_file.construct(env, md_alloc, Linker::file(path), true);
			Object::init(Linker::file(path), *_elf_file);
			return true;
		}

		bool const _elf_object_initialized;

		Dynamic _dyn;

	public:

		Elf_object(Dependency const &dep, Object::Name const &name,
		           Elf::Addr reloc_base)
		:
			_elf_object_initialized(_object_init(name, reloc_base)),
			_dyn(dep)
		{ }

		Elf_object(Env &env, Allocator &md_alloc, char const *path,
		           Dependency const &dep, Keep keep)
		:
			_keep(keep),
			_elf_object_initialized(_init_elf_file(env, md_alloc, path)),
			_dyn(md_alloc, dep, *this, &_elf_file->phdr)
		{
			/* register for static construction and relocation */
			Init::list()->insert(this);
			obj_list()->enqueue(this);

			/* add to link map */
			Debug::state_change(Debug::ADD, nullptr);
			setup_link_map();
			Debug::state_change(Debug::CONSISTENT, &_map);
		}

		virtual ~Elf_object()
		{
			if (!_file)
				return;

			if (verbose_loading)
				log("LD: destroy ELF object: ", name());

			/* remove from link map */
			Debug::state_change(Debug::DELETE, &_map);
			Link_map::remove(&_map);
			Debug::state_change(Debug::CONSISTENT, nullptr);

			/* remove from loaded objects list */
			obj_list()->remove(this);
		}

		/**
		 * Return symbol of given number from ELF
		 */
		Elf::Sym const *symbol(unsigned sym_index) const
		{
			return _dyn.symbol(sym_index);
		}

		// XXX remove this accessor?
		void link_map_addr(addr_t addr) { _map.addr = addr; }

		/**
		 * Return name of given symbol
		 */
		char const *symbol_name(Elf::Sym const &sym) const
		{
			return _dyn.symbol_name(sym);
		}

		Elf::Sym const *lookup_symbol(char const *name, unsigned long hash) const
		{
			return _dyn.lookup_symbol(name, hash);
		}

		/**
		 * Fill-out link map infos for this ELF
		 */
		void setup_link_map()
		{
			_map.addr    = _file ? _file->start + reloc_base() : reloc_base();
			_map.path    = name();
			_map.dynamic = _dyn.dynamic_ptr();

			Link_map::add(&_map);
		};

		Link_map const &link_map() const override { return _map; }
		Dynamic  const &dynamic()  const override { return _dyn; }

		void relocate_global() { _dyn.relocate_non_plt(BIND_NOW, Dynamic::SECOND_PASS); }

		void plt_setup() { _dyn.plt_setup(); }

		void update_dependency(Dependency const &dep) { _dyn.dep(dep); }

		void relocate(Bind bind) override
		{
			if (!_relocated)
				_dyn.relocate(bind);

			_relocated = true;
		}

		addr_t base_addr() const { return _map.addr; }

		Symbol_info symbol_at_address(addr_t addr) const override
		{
			Elf::Sym const sym = _dyn.symbol_by_addr(addr);

			return { _reloc_base + sym.st_value, _dyn.symbol_name(sym) };
		}

		/**
		 * Next in initializion list
		 */
		Object *next_init() const override {
			return List<Object>::Element::next();
		}

		/**
		 * Next in object list
		 */
		Object *next_obj() const override {
			return Fifo<Elf_object>::Element::next();
		}

		/**
		 * Object list
		 */
		static Fifo<Elf_object> *obj_list()
		{
			static Fifo<Elf_object> _list;
			return &_list;
		}

		void load()   override { _ref_count++; }
		bool unload() override { return (_keep == DONT_KEEP) && !(--_ref_count); }

		bool is_linker() const override { return false; }
		bool is_binary() const override { return false; }
};


/**
 * The dynamic linker object (ld.lib.so)
 */
struct Linker::Ld : Dependency, Elf_object
{
	Ld() :
		Dependency(*this, nullptr),
		Elf_object(*this, linker_name(), relocation_address())
	{ }

	void setup_link_map()
	{
		Elf_object::setup_link_map();

		link_map_addr(dynamic().link_map_addr());
	}

	void load_phdr(Env &env, Allocator &md_alloc)
	{
		_file = new (md_alloc) Elf_file(env, md_alloc, name(), false);
	}

	static Ld &linker();

	bool is_linker() const override { return true; }

	/**
	 * Entry point for jump relocations, it is called from assembly code and is implemented
	 * right below)
	 */
	static Elf::Addr jmp_slot(Dependency const &dep, Elf::Size index) asm("jmp_slot");
};


Elf::Addr Ld::jmp_slot(Dependency const &dep, Elf::Size index)
{
	Lock::Guard guard(lock());

	if (verbose_relocation)
		log("LD: SLOT ", &dep.obj(), " ", Hex(index));

	try {
		Reloc_jmpslot slot(dep, dep.obj().dynamic().pltrel_type(), 
		                   dep.obj().dynamic().pltrel(), index);
		return slot.target_addr();
	} catch (...) { error("LD: jump slot relocation failed. FATAL!"); }

	return 0;
}


/**
 * Linker object used during bootstrapping on stack (see: 'init_rtld')
 */
Linker::Ld &Linker::Ld::linker()
{
	/**
	 * Ld object with different vtable typeinfo
	 */
	struct Ld_vtable : Ld
	{
		Ld_vtable()
		{
			Elf_object::obj_list()->enqueue(this);
			plt_setup();
		}
	};
	static Ld_vtable _linker;
	return _linker;
}


/**
 * The dynamic binary to load
 */
struct Linker::Binary : Root_object, Elf_object
{
	Binary(Env &env, Allocator &md_alloc, Bind bind)
	:
		Root_object(md_alloc),
		Elf_object(env, md_alloc, binary_name(),
		           *new (md_alloc) Dependency(*this, this), KEEP)
	{
		/* create dep for binary and linker */
		Dependency *binary = const_cast<Dependency *>(&dynamic().dep());
		Root_object::enqueue(*binary);
		Dependency *linker = new (md_alloc) Dependency(Ld::linker(), this);
		Root_object::enqueue(*linker);

		Ld::linker().update_dependency(*linker);

		/* place linker on second place in link map */
		Ld::linker().setup_link_map();

		/* load dependencies */
		binary->load_needed(env, md_alloc, deps(), DONT_KEEP);

		/* relocate and call constructors */
		Init::list()->initialize(bind);
	}

	Elf::Addr lookup_symbol(char const *name)
	{
		Elf::Sym const *symbol = 0;

		if ((symbol = Elf_object::lookup_symbol(name, Hash_table::hash(name))))
			return reloc_base() + symbol->st_value;

		return 0;
	}

	void call_entry_point(Env &env)
	{
		/* call static construtors and register destructors */
		Func * const ctors_start = (Func *)lookup_symbol("_ctors_start");
		Func * const ctors_end   = (Func *)lookup_symbol("_ctors_end");
		for (Func * ctor = ctors_end; ctor != ctors_start; (*--ctor)());

		Func * const dtors_start = (Func *)lookup_symbol("_dtors_start");
		Func * const dtors_end   = (Func *)lookup_symbol("_dtors_end");
		for (Func * dtor = dtors_start; dtor != dtors_end; genode_atexit(*dtor++));

		/* call component entry point */
		/* XXX the function type for call_component_construct() is a candidate
		 * for a base-internal header */
		typedef void (*Entry)(Env &);
		Entry const entry = reinterpret_cast<Entry>(_file->entry);

		entry(env);
	}

	void relocate(Bind bind) override
	{
		/* relocate ourselves */
		Elf_object::relocate(bind);

		/*
		 * After having loaded the main program, we relocate the linker's
		 * symbols again such that, for example type informations, which are
		 * also present within the main program, become relocated to the correct
		 * positions
		 */
		Ld::linker().relocate_global();
	}

	bool is_binary() const override { return true; }
};


/***************************************
 ** Global Linker namespace functions **
 ***************************************/

Object &Linker::load(Env &env, Allocator &md_alloc, char const *path,
                     Dependency &dep, Keep keep)
{
	for (Object *e = Elf_object::obj_list()->head(); e; e = e->next_obj()) {

		if (verbose_loading)
			log("LOAD: ", Linker::file(path), " == ", e->name());

		if (!strcmp(Linker::file(path), e->name())) {
			e->load();
			return *e;
		}
	}

	return *new (md_alloc) Elf_object(env, md_alloc, path, dep, keep);
}


Object *Linker::obj_list_head()
{
	return Elf_object::obj_list()->head();
}


Elf::Sym const *Linker::lookup_symbol(unsigned sym_index, Dependency const &dep,
                                      Elf::Addr *base, bool undef, bool other)
{
	Elf_object const &elf    = static_cast<Elf_object const &>(dep.obj());
	Elf::Sym   const *symbol = elf.symbol(sym_index);

	if (!symbol) {
		warning("LD: unknown symbol index ", Hex(sym_index));
		return 0;
	}

	if (symbol->bind() == STB_LOCAL) {
		*base = dep.obj().reloc_base();
		return symbol;
	}

	return lookup_symbol(elf.symbol_name(*symbol), dep, base, undef, other);
}


Elf::Sym const *Linker::lookup_symbol(char const *name, Dependency const &dep,
                                      Elf::Addr *base, bool undef, bool other)
{
	Dependency const *curr        = &dep.first();
	unsigned long     hash        = Hash_table::hash(name);
	Elf::Sym   const *weak_symbol = 0;
	Elf::Addr        weak_base    = 0;
	Elf::Sym   const *symbol      = 0;

	//TODO: handle vertab and search in object list
	for (;curr; curr = curr->next()) {

		if (other && curr == &dep)
			continue;

		Elf_object const &elf = static_cast<Elf_object const &>(curr->obj());

		if ((symbol = elf.lookup_symbol(name, hash)) && (symbol->st_value || undef)) {

			if (dep.root() && verbose_lookup)
				log("LD: lookup ", name, " obj_src ", elf.name(),
				    " st ", symbol, " info ", Hex(symbol->st_info),
				    " weak: ", symbol->weak());

			if (!undef && symbol->st_shndx == SHN_UNDEF)
				continue;

			if (!symbol->weak() && symbol->st_shndx != SHN_UNDEF) {
				*base = elf.reloc_base();
				return symbol;
			}

			if (!weak_symbol) {
				weak_symbol = symbol;
				weak_base   = elf.reloc_base();
			}
		}
	}

	/* try searching binary's dependencies */
	if (!weak_symbol && dep.root()) {
		if (binary_ptr && &dep != binary_ptr->first_dep()) {
			return lookup_symbol(name, *binary_ptr->first_dep(), base, undef, other);
		} else {
			error("LD: could not lookup symbol \"", name, "\"");
			throw Not_found();
		}
	}

	if (dep.root() && verbose_lookup)
		log("LD: return ", weak_symbol);

	if (!weak_symbol)
		throw Not_found();

	*base = weak_base;
	return weak_symbol;
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
	 * type relocation might produce a wrong vtable pointer (at least on ARM), do
	 * not call any virtual funtions of this object.
	 */
	Ld linker_on_stack;
	linker_on_stack.relocate(BIND_LAZY);

	/*
	 * Create actual linker object with different vtable type and set PLT to new
	 * DAG.
	 */
	Ld::linker();
}


Genode::size_t Component::stack_size() { return 16*1024*sizeof(long); }


class Linker::Config
{
	private:

		Bind _bind    = BIND_LAZY;
		bool _verbose = false;

	public:

		Config(Env &env)
		{
			try {
				Attached_rom_dataspace config(env, "config");

				if (config.xml().attribute_value("ld_bind_now", false))
					_bind = BIND_NOW;

				_verbose = config.xml().attribute_value("ld_verbose", false);
			} catch (Rom_connection::Rom_connection_failed) { }
		}

		Bind bind()    const { return _bind; }
		bool verbose() const { return _verbose; }
};


static Genode::Lazy_volatile_object<Heap> &heap()
{
	return *unmanaged_singleton<Lazy_volatile_object<Heap>>();
}


void Genode::init_ldso_phdr(Env &env)
{
	heap().construct(env.ram(), env.rm());

	/* load program headers of linker now */
	if (!Ld::linker().file())
		Ld::linker().load_phdr(env, *heap());
}


void Component::construct(Genode::Env &env)
{
	/* read configuration */
	static Config config(env);
	verbose = config.verbose();

	/* load binary and all dependencies */
	try {
		binary_ptr = unmanaged_singleton<Binary>(env, *heap(), config.bind());
	} catch (...) {
		error("LD: failed to load program");
		throw;
	}

	/* print loaded object information */
	try {
		if (verbose) {
			using namespace Genode;
			log("  ",   Hex(Thread::stack_area_virtual_base()),
			    " .. ", Hex(Thread::stack_area_virtual_base() +
			                Thread::stack_area_virtual_size() - 1),
			    ": stack area");
			dump_link_map(*Elf_object::obj_list()->head());
		}
	} catch (...) {  }

	Link_map::dump();

	binary_ready_hook_for_gdb();

	/* start binary */
	binary_ptr->call_entry_point(env);
}
