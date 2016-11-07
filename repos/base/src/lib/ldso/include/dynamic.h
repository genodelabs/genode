/**
 * \brief  ELF-dynamic section (see ELF ABI)
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DYNAMIC_H_
#define _INCLUDE__DYNAMIC_H_

#include <relocation.h>

namespace Linker {
	struct Hash_table;
	struct Dynamic;
}

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

	Dependency const     *dep;
	Object     const     *obj;
	Elf::Dyn   const     *dynamic;

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

	Dynamic(Dependency const *dep)
	:
	  dep(dep), obj(dep->obj), dynamic((Elf::Dyn *)dynamic_address())
	{
		init();
	}

	Dynamic(Dependency const *dep, Object const *obj, Linker::Phdr const *phdr)
	:
		dep(dep), obj(obj), dynamic(find_dynamic(phdr))
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
					Genode::error("LD: Invalid PLT relocation ", (int)pltrel_type);
					throw Incompatible();
			}
		}

		relocate_non_plt();
	}

	void plt_setup()
	{
		if (pltgot)
			Plt_got r(dep, pltgot);
	}

	void relocate_non_plt(bool second_pass = false)
	{
		if (reloca)
			Reloc_non_plt r(dep, reloca, reloca_size);

		if (rel)
			Reloc_non_plt r(dep, rel, rel_size, second_pass);

		if (bind_now)
			Reloc_bind_now r(dep, pltrel, pltrel_size);
	}
};

#endif /* _INCLUDE__DYNAMIC_H_ */
