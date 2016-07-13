/**
 * \brief  Generic linker definitions
 * \author Sebastian Sumpf
 * \date   2014-10-24
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LINKER_H_
#define _INCLUDE__LINKER_H_

#include <base/exception.h>
#include <base/env.h>
#include <base/shared_object.h>
#include <util/fifo.h>
#include <util/misc_math.h>
#include <util/string.h>

#include <debug.h>
#include <elf.h>
#include <file.h>
#include <util.h>

/**
 * Debugging
 */
constexpr bool verbose_lookup     = false;
constexpr bool verbose_exception  = false;
constexpr bool verbose_shared     = false;
constexpr bool verbose_loading    = false;

extern Elf::Addr etext;

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
	 * Eager binding enable
	 */
	extern bool bind_now;

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
	Elf::Sym const *lookup_symbol(unsigned sym_index, Dependency const *, Elf::Addr *base,
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
	Elf::Sym const *lookup_symbol(char const *name, Dependency const *dep, Elf::Addr *base,
	                              bool undef = false, bool other = false);

	/**
	 * Load an ELF (setup segments and map program header)
	 *
	 * \param path  File to load
	 * \param dep   Dependency entry for new object
	 * \param flags 'Genode::Shared_object::KEEP' will not unload the ELF, if the
	 *              reference count reaches zero
	 *
	 * \return Linker::Object
	 */
	Object *load(char const *path, Dependency *dep, unsigned flags = 0);

	/**
	 * Returns the head of the global object list
	 */
	Object *obj_list_head();

	/**
	 * Returns the root-dependeny of the dynamic binary
	 */
	Dependency *binary_root_dep();

	/**
	 * Force to map the program header of the dynamic linker
	 */
	void load_linker_phdr();

	/**
	 * Exceptions
	 */
	class Incompatible : Genode::Exception  { };
	class Invalid_file : Genode::Exception  { };
	class Not_found    : Genode::Exception  { };

	/**
	 * Invariants
	 */
	constexpr char const *binary_name() { return "binary"; }
	constexpr char const *linker_name() { return "ld.lib.so"; }
}


/**
 * Shared object or binary
 */
class Linker::Object : public Genode::Fifo<Object>::Element,
                       public Genode::List<Object>::Element
{
	protected:

		enum { MAX_PATH = 128 };

		char        _name[MAX_PATH];
		File const *_file = nullptr;
		Elf::Addr   _reloc_base = 0;

	public:

		Object(Elf::Addr reloc_base) : _reloc_base(reloc_base) { }
		Object(char const *path, File const *file)
		: _file(file), _reloc_base(file->reloc_base)
		{
			Genode::strncpy(_name, Linker::file(path), MAX_PATH);
		}

		virtual ~Object()
		{
			if (_file)
				destroy(Genode::env()->heap(), const_cast<File *>(_file));
		}

		Elf::Addr reloc_base() const { return _reloc_base; }
		char const *name() const { return _name; }

		File      const *file() { return _file; }
		Elf::Size const  size() const { return _file ? _file->size : 0; }

		virtual bool is_linker() const = 0;
		virtual bool is_binary() const = 0;

		virtual void relocate() = 0;

		virtual void load() = 0;
		virtual bool unload() { return false;}

		/**
		 * Next object in global object list
		 */
		virtual Object *next_obj()  const = 0;

		/**
		 * Next object in initialization list
		 */
		virtual Object *next_init() const = 0;

		/**
		 * Return dynamic section of ELF
		 */
		virtual Dynamic  *dynamic()  = 0;

		/**
		 * Return link map for ELF
		 */
		virtual Link_map *link_map() = 0;

		/**
		 * Return address info for symboal at addr
		 */
		virtual void info(Genode::addr_t addr, Genode::Address_info &info) = 0;
	
		/**
		 * Global ELF access lock
		 */
		static Genode::Lock & lock()
		{
			static Genode::Lock _lock;
			return _lock;
		}
};


/**
 * Dependency of object
 */
struct Linker::Dependency : Genode::Fifo<Dependency>::Element
{
	Object      *obj   = nullptr; 
	Root_object *root  = nullptr;

	Dependency(Object *obj, Root_object *root) : obj(obj), root(root) { }

	Dependency(char const *path, Root_object *root, Genode::Fifo<Dependency> * const dep, 
	    unsigned flags = 0);
	~Dependency();

	/**
	 * Load dependent ELF object
	 */
	void load_needed(Genode::Fifo<Dependency> * const dep, unsigned flags = 0);

	/**
	 * Check if file is in this dependency tree
	 */
	bool in_dep(char const *file, Genode::Fifo<Dependency>  *const dep);
};


/**
 * Root of dependencies
 */
struct Linker::Root_object
{
	Genode::Fifo<Dependency> dep;

	/* main root */
	Root_object() { };

	/* runtime loaded root components */
	Root_object(char const *path, unsigned flags = 0);

	~Root_object()
	{
		Dependency *d;
		while ((d = dep.dequeue()))
			destroy(Genode::env()->heap(), d);
	}

	Link_map const *link_map() const
	{
		return dep.head()->obj->link_map();
	}
};

#endif /* _INCLUDE__LINKER_H_ */
