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
#include <util/fifo.h>
#include <util/misc_math.h>
#include <util/string.h>
#include <elf.h>
#include <trace.h>

/**
 * Debugging
 */
constexpr bool verbose_lookup     = false;
constexpr bool verbose_link_map   = false;
constexpr bool verbose_relocation = false;
constexpr bool verbose_exception  = false;
constexpr bool verbose_shared     = false;
constexpr bool verbose_loading    = false;

/**
 * Forward declartions and helpers
 */
namespace Linker
{
	class  Object;
	struct Phdr;
	struct File;
	struct Root_object;
	struct Dag;
	struct Elf_object;

	/**
	 * Find symbol via index
	 *
	 * \param sym_index  Symbol index within object
	 * \param dag        Directed acyclic graph of object
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
	Elf::Sym const *locate_symbol(unsigned sym_index, Dag const *, Elf::Addr *base,
	                              bool undef = false, bool other = false);


	/**
	 * Find symbol via name
	 *
	 * \param name       Symbol name
	 * \param dag        Directed acyclic graph of object
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
	Elf::Sym const *search_symbol(char const *name, Dag const *dag, Elf::Addr *base,
	                              bool undef = false, bool other = false);

	/**
	 * Load object
	 *
	 * \param path  Path of object
	 * \param load  True, load binary; False, load ELF header only
	 * 
	 * \throw Invalid_file     Segment is neither read-only/executable or read/write
	 * \throw Region_conflict  There is already something at the given address
	 * \throw Incompatible     Not an ELF
	 *
	 * \return File descriptor
	 */
	File     const *load(char const *path, bool load = true); 

	/**
	 * Exceptions
	 */
	class Incompatible : Genode::Exception  { };
	class Invalid_file : Genode::Exception  { };
	class Not_found    : Genode::Exception  { };

	/**
	 * Page handling
	 */
	template <typename T>
	static inline T trunc_page(T addr) {
		return addr & Genode::_align_mask((T)12); }

	template <typename T>
	static inline T round_page(T addr) {
		return Genode::align_addr(addr, (T)12); }

	/**
	 * Extract file name from path
	 */
	inline char const *file(char const *path)
	{
		/* strip directories */
			char  const *f, *r = path;
			for (f = r; *f; f++)
				if (*f == '/')
					r = f + 1;
			return r;
	}

	/**
	 * Invariants
	 */
	constexpr char const *binary_name() { return "binary"; }
	constexpr char const *linker_name() { return "ld.lib.so"; }
}


struct Linker::Phdr
{
	enum { MAX_PHDR = 10 };

	Elf::Phdr  phdr[MAX_PHDR];
	unsigned   count = 0;
};


struct Linker::File
{
	typedef void (*Entry)(void);

	Phdr       phdr;
	Entry      entry;
	Elf::Addr  reloc_base = 0;
	Elf::Addr  start      = 0;
	Elf::Size  size       = 0;

	virtual ~File() { }

	Elf::Phdr const *elf_phdr(unsigned index) const
	{
		if (index < phdr.count)
			return &phdr.phdr[index];

		return 0;
	}

	unsigned elf_phdr_count() const { return phdr.count; }

};


class Linker::Object : public Genode::Fifo<Object>::Element
{
	protected:

		enum { MAX_PATH = 128 };

		char        _name[MAX_PATH];
		File const *_file = nullptr;

	public:

		Object() { }
		Object(char const *path, File const *file)
		: _file(file)
		{
			Genode::strncpy(_name, Linker::file(path), MAX_PATH);
		}

		virtual ~Object()
		{
			if (_file)
				destroy(Genode::env()->heap(), const_cast<File *>(_file));
		}

		Elf::Addr  reloc_base() const { return _file ? _file->reloc_base : 0; }
		char const *name()      const { return _name; }

		File const *file() { return _file; }

		virtual bool is_linker() const = 0;
		virtual bool is_binary() const = 0;

		Elf::Size const size() const { return _file ? _file->size : 0; }
};


struct Linker::Dag : Genode::Fifo<Dag>::Element
{
	Object      *obj   = nullptr;
	Root_object *root  = nullptr;

	Dag(Object *obj, Root_object *root) : obj(obj), root(root) { }

	Dag(char const *path, Root_object *root, Genode::Fifo<Dag> * const dag, 
	    unsigned flags = 0);
	~Dag();

	void load_needed(Genode::Fifo<Dag> * const dag, unsigned flags = 0);
	bool in_dag(char const *file, Genode::Fifo<Dag>  *const dag);
};


static inline bool verbose_reloc(Linker::Dag const *d)
{
	return d->root && verbose_relocation;
}

extern "C" void _jmp_slot(void);

#endif /* _INCLUDE__LINKER_H_ */
