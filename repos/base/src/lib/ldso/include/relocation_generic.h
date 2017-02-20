/**
 * \brief  Generic relocation classes
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RELOCATION_GENERIC_H_
#define _INCLUDE__RELOCATION_GENERIC_H_

#include <linker.h>

constexpr bool verbose_relocation = false;

static inline bool verbose_reloc(Linker::Dependency const &d)
{
	return d.root() && verbose_relocation;
}

/**
 * Low level linker entry for jump slot relocations
 */
extern "C" void _jmp_slot(void);


namespace Linker
{
	struct Plt_got;
	template <typename REL, unsigned TYPE, bool DIV> class Reloc_jmpslot_generic;
	template <typename REL, unsigned TYPE, unsigned JMPSLOT> struct Reloc_plt_generic;
	template <typename REL, unsigned TYPE> struct Reloc_bind_now_generic;
	class Reloc_non_plt_generic;
}


/**
 * Set 2nd and 3rd GOT entry (see: SYSTEM V APPLICATION BINARY INTERFACE
 * Intel386 Architecture Processor Supplement - 5.9
 */
struct Linker::Plt_got
{
	Plt_got(Dependency const &dep, Elf::Addr *pltgot)
	{
		if (verbose_relocation)
			log("OBJ: ", dep.obj().name(), " (", &dep, ")");

		pltgot[1] = (Elf::Addr) &dep;       /* ELF object */
		pltgot[2] = (Elf::Addr) &_jmp_slot; /* Linker entry */
	}
};


/**
 * PLT relocations
 */
template<typename REL, unsigned TYPE, unsigned JMPSLOT>
struct Linker::Reloc_plt_generic
{
	Reloc_plt_generic(Object const &obj, D_tag const type,
	                  Elf::Rel const *start, unsigned long size)
	{
		if (type != TYPE) {
			error("LD: Unsupported PLT relocation type: ", (int)type);
			throw Incompatible();
		}

		REL const *rel = (REL const *)start;
		REL const *end = rel + (size / sizeof(REL));
		for (; rel < end; rel++) {

			if (rel->type() != JMPSLOT) {
				error("LD: Unsupported PLT relocation ", (int)rel->type());
				throw Incompatible();
			}

			/* find relocation address and add relocation base */
			Elf::Addr *addr = (Elf::Addr *)(obj.reloc_base() + rel->offset);
			*addr          += obj.reloc_base();
		}
	}
};


class Linker::Reloc_non_plt_generic
{
	protected:

		Dependency const &_dep;

		/**
		 * Copy relocations, these are just for the main program, we can do them
		 * safely here since all other DSO are loaded, relocated, and constructed at
		 * this point
		 */
		template <typename REL>
		void _copy(REL const *rel, Elf::Addr *addr)
		{
			if (!_dep.obj().is_binary()) {
				error("LD: copy relocation in DSO "
				      "(", _dep.obj().name(), " at ", addr, ")");
				throw Incompatible();
			}

			Elf::Sym const *sym;
			Elf::Addr      reloc_base;

			 /* search symbol in other objects, do not return undefined symbols */
			if (!(sym = lookup_symbol(rel->sym(), _dep, &reloc_base, false, true))) {
				warning("LD: symbol not found");
				return;
			}

			Elf::Addr src = reloc_base + sym->st_value;
			memcpy(addr, (void *)src, sym->st_size);

			if (verbose_relocation)
				log("Copy relocation: ", Hex(src),
				    " -> ", addr, " (", Hex(sym->st_size), " bytes)"
				    " val: ", Hex(sym->st_value));
		}

	public:

		Reloc_non_plt_generic(Dependency const &dep) : _dep(dep) { }
};


/**
 * Generic jmp slot handling
 */
template <typename REL, unsigned TYPE, bool DIV>
class Linker::Reloc_jmpslot_generic
{
		Elf::Addr *_addr = 0;

	public:

		Reloc_jmpslot_generic(Dependency const &dep, unsigned const type, Elf::Rel const* pltrel,
		                      Elf::Size const index)
		{
			if (type != TYPE) {
				error("LD: unsupported JMP relocation type: ", (int)type);
				throw Incompatible();
			}
		
			REL const *rel = &((REL *)pltrel)[index / (DIV ? sizeof(REL) : 1)];
			Elf::Sym const *sym;
			Elf::Addr      reloc_base;

			if (!(sym = lookup_symbol(rel->sym(), dep, &reloc_base))) {
				warning("LD: symbol not found");
				return;
			}

			/* write address of symbol to jump slot */
			_addr  = (Elf::Addr *)(dep.obj().reloc_base() + rel->offset);
			*_addr = reloc_base + sym->st_value;


			if (verbose_relocation) {
				log("jmp: rbase ", Hex(reloc_base),
				    " s: ", sym, " sval: ", Hex(sym->st_value));
				log("jmp_slot at ", _addr, " -> ", *_addr);
			}
		}

		Elf::Addr target_addr() const { return *_addr; }
};


/**
 * Relocate jump slots immediately
 */
template <typename REL, unsigned TYPE>
struct Linker::Reloc_bind_now_generic
{
	Reloc_bind_now_generic(Dependency const &dep, Elf::Rel const *pltrel, unsigned long const size)
	{
		Elf::Size last_index = size / sizeof(REL);

		for (Elf::Size index = 0; index < last_index; index++)
			Reloc_jmpslot_generic<REL, TYPE, false> reloc(dep, TYPE, pltrel, index);
	}
};

#endif /* _INCLUDE__RELOCATION_GENERIC_H_ */
