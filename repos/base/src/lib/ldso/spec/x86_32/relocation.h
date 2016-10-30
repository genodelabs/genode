/**
 * \brief  x86_32 specific relocations
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIB__LDSO__SPEC__X86_32__RELOCATION_H_
#define _LIB__LDSO__SPEC__X86_32__RELOCATION_H_

#include <relocation_generic.h>
#include <dynamic_generic.h>

namespace Linker {

	enum Reloc_types {
		R_32       = 1,
		R_COPY     = 5,
		R_GLOB_DAT = 6,
		R_JMPSLOT  = 7,
		R_RELATIVE = 8,
	};

	class Reloc_non_plt;

	typedef Reloc_plt_generic<Elf::Rel, DT_REL, R_JMPSLOT> Reloc_plt;
	typedef Reloc_jmpslot_generic<Elf::Rel, DT_REL, true>  Reloc_jmpslot;
	typedef Reloc_bind_now_generic<Elf::Rel, DT_REL>       Reloc_bind_now;
}


class Linker::Reloc_non_plt : public Reloc_non_plt_generic
{
	private:

		void _glob_dat(Elf::Rel const *rel, Elf::Addr *addr, bool addend = false)
		{
			Elf::Addr reloc_base;
			Elf::Sym  const *sym;

			if (!(sym = lookup_symbol(rel->sym(), _dep, &reloc_base)))
				return;

			*addr = (addend ? *addr : 0) + reloc_base + sym->st_value;
		}

		void _relative(Elf::Rel const *rel, Elf::Addr *addr)
		{
			if (_dep.obj().reloc_base())
				*addr += _dep.obj().reloc_base();
		}

	public:

		Reloc_non_plt(Dependency const &dep, Elf::Rela const *, unsigned long)
		: Reloc_non_plt_generic(dep)
		{
			error("LD: DT_RELA not supported");
			throw Incompatible();
		}

		Reloc_non_plt(Dependency const &dep, Elf::Rel const *rel, unsigned long size,
		              bool second_pass)
		: Reloc_non_plt_generic(dep)
		{
			Elf::Rel const *end = rel + (size / sizeof(Elf::Rel));

			for (; rel < end; rel++) {
				Elf::Addr *addr = (Elf::Addr *)(_dep.obj().reloc_base() + rel->offset);

				if (second_pass && rel->type() != R_GLOB_DAT)
					continue;

				switch (rel->type()) {

					case R_32      : _glob_dat(rel, addr, true); break;
					case R_GLOB_DAT: _glob_dat(rel, addr);       break;
					case R_COPY    : _copy<Elf::Rel>(rel, addr); break;
					case R_RELATIVE: _relative(rel, addr);       break;
					default:
						if (_dep.root()) {
							warning("LD: Unkown relocation ", (int)rel->type());
							throw Incompatible();
						}
						break;
				}
			}
		}
};

#endif /* _LIB__LDSO__SPEC__X86_32__RELOCATION_H_ */
