/**
 * \brief  x86_64  specific relocations
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIB__LDSO__SPEC__X86_64__RELOCATION_H_
#define _LIB__LDSO__SPEC__X86_64__RELOCATION_H_

#include <relocation_generic.h>
#include <dynamic_generic.h>

namespace Linker {

/**
 * Relocation types
 */
	enum Reloc_types {
		R_64       = 1, /* add 64 bit symbol value.       */
		R_COPY     = 5,
		R_GLOB_DAT = 6, /* GOT entry to data address      */
		R_JMPSLOT  = 7, /* jump slot                      */
		R_RELATIVE = 8, /* add load addr of shared object */
	};

	class Reloc_non_plt;

	typedef Reloc_plt_generic<Elf::Rela, DT_RELA, R_JMPSLOT> Reloc_plt;
	typedef Reloc_jmpslot_generic<Elf::Rela, DT_RELA, false> Reloc_jmpslot;
	typedef Reloc_bind_now_generic<Elf::Rela, DT_RELA>       Reloc_bind_now;
};

class Linker::Reloc_non_plt : public Reloc_non_plt_generic
{
	private:
		/**
		 * Relative relocation (reloc base + addend)
		 */
		void _relative(Elf::Rela const *rel, Elf::Addr *addr)
		{
			*addr = _dep->obj->reloc_base() + rel->addend;
		}

		/**
		 * GOT entry to data address or 64 bit symbol (addend = true),
		 * (reloc base of containing ojbect + symbol value (+ addend)
		 */
		void _glob_dat_64(Elf::Rela const *rel, Elf::Addr *addr, bool addend)
		{
			Elf::Addr reloc_base;
			Elf::Sym  const *sym;

			if (!(sym = lookup_symbol(rel->sym(), _dep, &reloc_base)))
				return;

			*addr = reloc_base + sym->st_value + (addend ? rel->addend : 0);
			if (verbose_reloc(_dep))
				Genode::log("GLOB DAT ", addr, " -> ", *addr,
				            " r ", reloc_base, " v ", sym->st_value);
		}

	public:

		Reloc_non_plt(Dependency const *dep, Elf::Rela const *rel, unsigned long size)
		: Reloc_non_plt_generic(dep)
		{
			Elf::Rela const *end = rel + (size / sizeof(Elf::Rela));

			for (; rel < end; rel++) {
				Elf::Addr *addr = (Elf::Addr *)(_dep->obj->reloc_base() + rel->offset);

				switch(rel->type()) {
					case R_64:       _glob_dat_64(rel, addr, true);  break;
					case R_GLOB_DAT: _glob_dat_64(rel, addr, false); break;
					case R_COPY:     _copy<Elf::Rela>(rel, addr);    break;
					case R_RELATIVE: _relative(rel, addr);           break;

					default:
						if (!_dep->obj->is_linker()) {
							Genode::warning("LD: Unkown relocation ", (int)rel->type());
							throw Incompatible();
						}
						break;
				}
			}
		}

		Reloc_non_plt(Dependency const *dep, Elf::Rel const *, unsigned long, bool)
		: Reloc_non_plt_generic(dep)
		{
			Genode::error("LD: DT_REL not supported");
			throw Incompatible();
		}
};

#endif /* _LIB__LDSO__SPEC__X86_64__RELOCATION_H_ */
