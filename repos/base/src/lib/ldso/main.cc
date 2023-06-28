/**
 * \brief  Genode's dynamic linker
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <util/list.h>
#include <util/string.h>
#include <base/thread.h>
#include <base/heap.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>
#include <base/internal/globals.h>

/* local includes */
#include <dynamic.h>
#include <init.h>
#include <region_map.h>
#include <config.h>

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
static    Parent *parent_ptr = nullptr;
bool      Linker::verbose  = false;
Stage     Linker::stage    = STAGE_BINARY;
Link_map *Link_map::first;


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


Genode::Mutex &Linker::mutex()
{
	static Mutex _mutex;
	return _mutex;
}


Genode::Mutex &Linker::shared_object_mutex()
{
	static Mutex _mutex;
	return _mutex;
}


/**************************************************************
 ** ELF object types (shared object, dynamic binaries, ldso  **
 **************************************************************/

Object::Object_list &Object::_object_list()
{
	static Object_list _list;
	return _list;
}


/**
 * The actual ELF object, one per file
 */
class Linker::Elf_object : public Object, private Fifo<Elf_object>::Element
{
	private:

		friend class Fifo<Elf_object>;

		Link_map _map       { };
		unsigned _ref_count { 1 };
		Keep     _keep      { KEEP };
		bool     _relocated { false };

		/*
		 * Optional ELF file, skipped for initial 'Ld' initialization
		 */
		Constructible<Elf_file> _elf_file { };


		bool _object_init(char const *name, Elf::Addr reloc_base)
		{
			Object::init(name, reloc_base);
			return true;
		}

		bool _init_elf_file(Env &env, Allocator &md_alloc, char const *path)
		{
			_elf_file.construct(env, md_alloc, path, true);
			Object::init(Linker::file(path), *_elf_file);
			return true;
		}

		bool const _elf_object_initialized;

		Dynamic _dyn;

	public:

		Elf_object(Dependency const &dep, char const *name,
		           Elf::Addr reloc_base) SELF_RELOC
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
			with_object_list([&] (Object_list &list) {
				list.enqueue(*this); });

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
			with_object_list([&] (Object_list &list) {
				list.remove(*this); });
			Init::list()->remove(this);
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

		void link_map_make_first()
		{
			Link_map::make_first(&_map);
		}

		void force_keep() override { _keep = KEEP; }

		Link_map const &link_map() const override { return _map; }
		Dynamic  const &dynamic()  const override { return _dyn; }

		void relocate_global() { _dyn.relocate_non_plt(BIND_NOW, Dynamic::SECOND_PASS); }

		void plt_setup() { _dyn.plt_setup(); }

		void update_dependency(Dependency const &dep) { _dyn.dep(dep); }

		void relocate(Bind bind) override SELF_RELOC
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
		Object *next_init() const override { return _next_object(); }

		void load()   override { _ref_count++; }
		bool unload() override { return (_keep == DONT_KEEP) && !(--_ref_count); }

		bool already_present() const override { return _ref_count > 1; }

		bool keep() const override { return _keep == KEEP; }

		bool is_linker() const override { return false; }
		bool is_binary() const override { return false; }
};


/**
 * The dynamic linker object (ld.lib.so)
 */
struct Linker::Ld : private Dependency, Elf_object
{
	Ld(bool use_name = true) SELF_RELOC :
		Dependency(*this, nullptr),
		Elf_object(*this, use_name ? linker_name() : nullptr, relocation_address())
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

	bool keep() const override { return true; }

	/**
	 * Entry point for jump relocations, it is called from assembly code and is implemented
	 * right below)
	 */
	static Elf::Addr jmp_slot(Dependency const &dep, Elf::Size index) asm("jmp_slot");
};


Elf::Addr Ld::jmp_slot(Dependency const &dep, Elf::Size index)
{
	try {
		Mutex::Guard guard(mutex());

		if (verbose_relocation)
			log("LD: SLOT ", &dep.obj(), " ", Hex(index));

		Reloc_jmpslot slot(dep, dep.obj().dynamic().pltrel_type(), 
		                   dep.obj().dynamic().pltrel(), index);
		return slot.target_addr();
	} catch (Linker::Not_found &symbol) {
		error("LD: jump slot relocation failed for symbol: '", symbol, "'");
		throw;
	} catch (...) {
		error("LD: jump slot relocation failed:: '", Current_exception(), "'");
		throw;
	}

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
			with_object_list([&] (Object_list &list) {
				list.enqueue(*this); });
			plt_setup();
		}
	};
	static Ld_vtable _linker;
	return _linker;
}


/**
 * The dynamic binary to load
 */
struct Linker::Binary : private Root_object, public Elf_object
{
	using Root_object::first_dep;

	bool const _check_ctors;

	bool static_construction_finished = false;

	Binary(Env &env, Allocator &md_alloc, Config const &config, char const *name)
	:
		Root_object(md_alloc),
		Elf_object(env, md_alloc, name,
		           *new (md_alloc) Dependency(*this, this), DONT_KEEP),
		_check_ctors(config.check_ctors())
	{
		/* create dep for binary and linker */
		Dependency *binary = const_cast<Dependency *>(&dynamic().dep());
		Root_object::enqueue(*binary);
		Dependency *linker = new (md_alloc) Dependency(Ld::linker(), this);
		Root_object::enqueue(*linker);

		Ld::linker().update_dependency(*linker);

		/* place linker on second place in link map */
		Ld::linker().setup_link_map();

		/* preload libraries specified in the configuration */
		binary->preload(env, md_alloc, deps(), config);

		/* load dependencies */
		binary->load_needed(env, md_alloc, deps(), DONT_KEEP);

		/* relocate and call constructors */
		Init::list()->initialize(config.bind(), STAGE_BINARY);
	}

	Elf::Addr lookup_symbol(char const *name)
	{
		try {
			Elf::Addr base = 0;
			Elf::Sym const *sym = Linker::lookup_symbol(name, dynamic().dep(), &base);
			return base + sym->st_value;
		}
		catch (Linker::Not_found) { return 0; }
	}

	bool static_construction_pending()
	{
		if (static_construction_finished) return false;

		return Init::list()->needs_static_construction();
	}

	void finish_static_construction()
	{
		Init::list()->exec_static_constructors();

		/* call static constructors */
		Func * const ctors_start = (Func *)lookup_symbol("_ctors_start");
		Func * const ctors_end   = (Func *)lookup_symbol("_ctors_end");
		for (Func * ctor = ctors_end; ctor != ctors_start; (*--ctor)());

		static_construction_finished = true;
		stage = STAGE_SO;
	}

	void call_dtors()
	{
		Func * const dtors_start = (Func *)lookup_symbol("_dtors_start");
		Func * const dtors_end   = (Func *)lookup_symbol("_dtors_end");
		for (Func * dtor = dtors_end; dtor != dtors_start; (*--dtor)());
	}

	void call_entry_point(Env &env)
	{
		/* apply the component-provided stack size */
		if (Elf::Addr addr = lookup_symbol("_ZN9Component10stack_sizeEv")) {

			/* call 'Component::stack_size()' */
			size_t const stack_size = ((size_t(*)())addr)();

			/* expand stack according to the component's needs */
			Thread::myself()->stack_size(stack_size);
		}

		/* call 'Component::construct' function if present */
		if (Elf::Addr addr = lookup_symbol("_ZN9Component9constructERN6Genode3EnvE")) {
			((void(*)(Env &))addr)(env);

			if (_check_ctors && static_construction_pending()) {
				error("Component::construct() returned without executing "
				      "pending static constructors (fix by calling "
				      "Genode::Env::exec_static_constructors())");
				throw Fatal();
			}

			stage = STAGE_SO;
			return;
		}

		error("dynamic linker: component-entrypoint lookup failed");
		throw Fatal();
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


void genode_exit(int status)
{
	binary_ptr->call_dtors();

	/* inform parent about the exit status */
	if (parent_ptr)
		parent_ptr->exit(status);

	/* wait for destruction by the parent */
	Genode::sleep_forever();
}


/**********************************
 ** Linker object implementation **
 **********************************/

Elf::Addr Linker::Object::_symbol_address(char const *name)
{
	unsigned long   hash = Hash_table::hash(name);
	Elf::Sym const *sym  = dynamic().lookup_symbol(name, hash);

	if (sym)
		return reloc_base() + sym->st_value;
	else
		return Elf::Addr(0);
}


bool Linker::Object::needs_static_construction()
{
	return _symbol_address("_ctors_end") != _symbol_address("_ctors_start");
}


/***************************************
 ** Global Linker namespace functions **
 ***************************************/

Object &Linker::load(Env &env, Allocator &md_alloc, char const *path,
                     Dependency &dep, Keep keep)
{
	Object *result = nullptr;
	Object::with_object_list([&] (Object::Object_list &list) {
		list.for_each([&] (Object &obj) {

			if (result == nullptr) {
				if (verbose_loading)
					log("LOAD: ", Linker::file(path), " == ", obj.name());

				if (!strcmp(Linker::file(path), obj.name())) {
					obj.load();
					result = &obj;
				}
			}
		});
	});

	if (result == nullptr)
		result = new (md_alloc) Elf_object(env, md_alloc, path, dep, keep);

	return *result;
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
			throw Not_found(name);
		}
	}

	if (dep.root() && verbose_lookup)
		log("LD: return ", weak_symbol);

	if (!weak_symbol)
		throw Not_found(name);

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
	Ld linker_on_stack { false };
	linker_on_stack.relocate(BIND_LAZY);

	/* init cxa guard mechanism before any local static variables are used */
	init_cxx_guard();

	/*
	 * Create actual linker object with different vtable type and set PLT to new
	 * DAG.
	 */
	Ld::linker();
}


static Genode::Constructible<Heap> &heap()
{
	return *unmanaged_singleton<Constructible<Heap>>();
}


void Genode::init_ldso_phdr(Env &env)
{
	/*
	 * Custom 'Region_map' that is used to place heap allocations of the
	 * dynamic linker within the linker area, keeping the rest of the
	 * component's virtual address space unpolluted.
	 */
	struct Linker_area_region_map : Region_map
	{
		struct Not_implemented : Exception { };

		Local_addr attach(Dataspace_capability ds, size_t, off_t,
		                  bool, Local_addr, bool, bool) override
		{
			size_t const size = Dataspace_client(ds).size();

			Linker::Region_map &linker_area = *Linker::Region_map::r();

			addr_t const at = linker_area.alloc_region_at_end(size);

			(void)linker_area.attach_at(ds, at, size, 0UL);

			return at;
		}

		void detach(Local_addr) override { throw Not_implemented(); }

		void fault_handler(Signal_context_capability) override { }

		State state() override { throw Not_implemented(); }

		Dataspace_capability dataspace() override { throw Not_implemented(); }

		Linker_area_region_map() { }
	};

	Linker_area_region_map &ld_rm = *unmanaged_singleton<Linker_area_region_map>();

	/*
	 * Use a statically allocated initial block to make the first dynamic
	 * allocations deterministic. This assumption is required by the libc's
	 * fork mechanism on Linux. Without the initial block, the Linux kernel
	 * would attach the heap's backing-store dataspaces to differently
	 * randomized addresses in the new process. The binary's GOT (containing
	 * pointers to the linker's heap-allocated objects) of the new process,
	 * however, is copied from the parent process. So the pointed-to objects
	 * must reside on the same addresses in the parent and child.
	 */
	static char initial_block[8*1024];
	heap().construct(&env.ram(), &ld_rm, Heap::UNLIMITED,
	                 initial_block, sizeof(initial_block));

	/* load program headers of linker now */
	if (!Ld::linker().file())
		Ld::linker().load_phdr(env, *heap());
}

void Genode::exec_static_constructors()
{
	binary_ptr->finish_static_construction();
}


void Genode::Dynamic_linker::_for_each_loaded_object(Env &, For_each_fn const &fn)
{
	Object::with_object_list([&] (Object::Object_list &list) {
		list.for_each([&] (Object const &obj) {

			Elf_file const *elf_file_ptr =
				obj.file() ? dynamic_cast<Elf_file const *>(obj.file()) : nullptr;

			if (!elf_file_ptr)
				return;

			elf_file_ptr->with_rw_phdr([&] (Elf::Phdr const &phdr) {

				Object_info info { .name     = obj.name(),
				                   .ds_cap   = elf_file_ptr->rom_cap,
				                   .rw_start = (void *)(obj.reloc_base() + phdr.p_vaddr),
				                   .rw_size  = phdr.p_memsz };

				fn.supply_object_info(info);
			});
		});
	});
}


void Dynamic_linker::keep(Env &, char const *binary)
{
	Object::with_object_list([&] (Object::Object_list &list) {
		list.for_each([&] (Object &obj) {
			if (Object::Name(binary) == obj.name())
				obj.force_keep(); });
	});
}


void *Dynamic_linker::_respawn(Env &env, char const *binary, char const *entry_name)
{
	Object::Name const name(binary);

	/* unload original binary */
	binary_ptr->~Binary();

	Config const config(env);

	/* load new binary */
	construct_at<Binary>(binary_ptr, env, *heap(), config, name.string());

	/* move to front of link map */
	binary_ptr->link_map_make_first();

	try {
		return (void *)binary_ptr->lookup_symbol(entry_name);
	}
	catch (...) { }

	throw Dynamic_linker::Invalid_symbol();
}


void Component::construct(Genode::Env &env)
{
	/* read configuration */
	Config const config(env);

	verbose = config.verbose();

	parent_ptr = &env.parent();

	/* load binary and all dependencies */
	try {
		binary_ptr = unmanaged_singleton<Binary>(env, *heap(), config, binary_name());
	} catch(Linker::Not_found &symbol) {
		error("LD: symbol not found: '", symbol, "'");
		throw;
	} catch (...) {
		error("LD: exception during program load: '", Current_exception(), "'");
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
			Object::with_object_list([] (Object::Object_list &list) {
				list.for_each([] (Object const &obj) {
					dump_link_map(obj); });
			});
		}
	} catch (...) {  }

	Link_map::dump();

	binary_ready_hook_for_gdb();

	binary_ready_hook_for_platform();

	/* start binary */
	binary_ptr->call_entry_point(env);
}
