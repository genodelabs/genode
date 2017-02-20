/*
 * \brief  Generic linker definitions
 * \author Sebastian Sumpf
 * \date   2014-10-24
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LINKER_H_
#define _INCLUDE__LINKER_H_

#include <types.h>
#include <debug.h>
#include <elf.h>
#include <file.h>
#include <util.h>

/*
 * Mark functions that are used during the linkers self-relocation phase as
 * always inline. Some platforms (riscv)  perform function calls through the
 * GOT that is not initialized (zero) at this state.
 */
#define SELF_RELOC __attribute__((always_inline))

/**
 * Forward declartions and helpers
 */
namespace Linker {
	class  Object;
	struct Root_object;
	struct Dependency;
	struct Elf_object;
	struct Dynamic;

	typedef void (*Func)(void);

	/**
	 * Print diagnostic information
	 *
	 * The value corresponds to the config attribute "ld_verbose".
	 */
	extern bool verbose;

	/**
	 * Find symbol via index
	 *
	 * \param sym_index  Symbol index within object
	 * \param dep        Dependency of object
	 * \param base       Returned address of symbol
	 * \param undef      True, return undefined symbol; False return defined
	 *                   symbols only
	 * \param other      True, search for symbol in other objects; False, search
	 *                   for symbol in given object as well.
	 *
	 * \throw Not_found  Symbol not found
	 *
	 * \return Symbol information
	 */
	Elf::Sym const *lookup_symbol(unsigned sym_index, Dependency const &, Elf::Addr *base,
	                              bool undef = false, bool other = false);

	/**
	 * Find symbol via name
	 *
	 * \param name       Symbol name
	 * \param dep        Dependency  of object
	 * \param base       Returned address of symbol
	 * \param undef      True, return undefined symbol; False return defined
	 *                   symbols only
	 * \param other      True, search for symbol in other objects; False, search
	 *                   for symbol in given object as well.
	 *
	 * \throw Not_found  Symbol not found
	 *
	 * \return Symbol information
	 */
	Elf::Sym const *lookup_symbol(char const *name, Dependency const &dep, Elf::Addr *base,
	                              bool undef = false, bool other = false);

	/**
	 * Load an ELF (setup segments and map program header)
	 *
	 * \param md_alloc  allocator used for dyamically allocater meta data
	 * \param path      rom module to load
	 * \param dep       dependency entry for new object
	 * \param flags     'Shared_object::KEEP' will not unload the ELF,
	 *                  if the reference count reaches zero
	 *
	 * \return Linker::Object
	 */
	Object &load(Env &, Allocator &md_alloc, char const *path, Dependency &dep,
	             Keep keep);

	/**
	 * Returns the head of the global object list
	 */
	Object *obj_list_head();

	/**
	 * Returns the root-dependeny of the dynamic binary
	 */
	Dependency *binary_root_dep();

	/**
	 * Global ELF access lock
	 */
	Lock &lock();
}


/**
 * Shared object or binary
 */
class Linker::Object : public Fifo<Object>::Element,
                       public List<Object>::Element
{
	public:

		typedef String<128> Name;

	protected:

		Name        _name;
		File const *_file = nullptr;
		Elf::Addr   _reloc_base = 0;

	public:

		void init(Name const &name, Elf::Addr reloc_base)
		{
			_name       = name;
			_reloc_base = reloc_base;
		}

		void init(Name const &name, File const &file)
		{
			_name       = name;
			_file       = &file;
			_reloc_base = file.reloc_base;
		}

		virtual ~Object() { }

		Elf::Addr        reloc_base() const { return _reloc_base; }
		char      const *name()       const { return _name.string(); }
		File      const *file()       const { return _file; }
		Elf::Size const  size()       const { return _file ? _file->size : 0; }

		virtual bool is_linker() const = 0;
		virtual bool is_binary() const = 0;

		virtual void relocate(Bind) = 0;

		virtual void load() = 0;
		virtual bool unload() { return false;}

		/**
		 * Next object in global object list
		 */
		virtual Object *next_obj() const = 0;

		/**
		 * Next object in initialization list
		 */
		virtual Object *next_init() const = 0;

		/**
		 * Return dynamic section of ELF
		 */
		virtual Dynamic const &dynamic() const = 0;

		/**
		 * Return link map for ELF
		 */
		virtual Link_map const &link_map() const = 0;

		/**
		 * Return type of 'symbol_at_address'
		 */
		struct Symbol_info { addr_t addr; char const *name; };

		/**
		 * Return address info for symboal at addr
		 */
		virtual Symbol_info symbol_at_address(addr_t addr) const = 0;
};


/**
 * Dependency of object
 */
class Linker::Dependency : public Fifo<Dependency>::Element, Noncopyable
{
	private:

		Object      &_obj;
		Root_object *_root     = nullptr;
		Allocator   *_md_alloc = nullptr;

		/**
		 * Check if file is in this dependency tree
		 */
		bool in_dep(char const *file, Fifo<Dependency> const &);

	public:

		/*
		 * Called by 'Ld' constructor
		 */
		Dependency(Object &obj, Root_object *root) : _obj(obj), _root(root) { }

		Dependency(Env &, Allocator &, char const *path, Root_object *,
		           Fifo<Dependency> &, Keep);

		~Dependency();

		/**
		 * Load dependent ELF object
		 */
		void load_needed(Env &, Allocator &, Fifo<Dependency> &, Keep);

		bool root() const { return _root != nullptr; }

		Object const &obj() const { return _obj; }

		/**
		 * Return first element of dependency list
		 */
		Dependency const &first() const;
};


/**
 * Root of dependencies
 */
class Linker::Root_object
{
	private:

		Fifo<Dependency> _deps;

		Allocator &_md_alloc;

	public:

		/* main root */
		Root_object(Allocator &md_alloc) : _md_alloc(md_alloc) { };

		/* runtime loaded root components */
		Root_object(Env &, Allocator &, char const *path, Bind, Keep);

		~Root_object()
		{
			while (Dependency *d = _deps.dequeue())
				destroy(_md_alloc, d);
		}

		Link_map const &link_map() const
		{
			return _deps.head()->obj().link_map();
		}

		Dependency const *first_dep() const { return _deps.head(); }

		void enqueue(Dependency &dep) { _deps.enqueue(&dep); }

		Fifo<Dependency> &deps() { return _deps; }
};

#endif /* _INCLUDE__LINKER_H_ */
