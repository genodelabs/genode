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

#ifndef _X86_32__RELOCATION_H_
#define _X86_32__RELOCATION_H_

#include <relocation_generic.h>

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

			if (!(sym = locate_symbol(rel->sym(), _dag, &reloc_base)))
				return;

			*addr = (addend ? *addr : 0) + reloc_base + sym->st_value;
			trace("REL32", (unsigned long)addr, *addr, 0);
		}

		void _relative(Elf::Rel const *rel, Elf::Addr *addr)
		{
			if (_dag->obj->reloc_base())
				*addr += _dag->obj->reloc_base();
		}

	public:

		Reloc_non_plt(Dag const *dag, Elf::Rela const *, unsigned long)
		: Reloc_non_plt_generic(dag)
		{
			PERR("LD: DT_RELA not supported");
			trace("Non_plt", 0, 0, 0);
			throw Incompatible();
		}

		Reloc_non_plt(Dag const *dag, Elf::Rel const *rel, unsigned long size, bool second_pass)
		: Reloc_non_plt_generic(dag)
		{
			Elf::Rel const *end = rel + (size / sizeof(Elf::Rel));

			for (; rel < end; rel++) {
				Elf::Addr *addr = (Elf::Addr *)(_dag->obj->reloc_base() + rel->offset);

				if (second_pass && rel->type() != R_GLOB_DAT)
					continue;

				switch (rel->type()) {

					case R_32      : _glob_dat(rel, addr, true); break;
					case R_GLOB_DAT: _glob_dat(rel, addr);       break;
					case R_COPY    : _copy<Elf::Rel>(rel, addr); break;
					case R_RELATIVE: _relative(rel, addr);       break;
					default:
						trace("UNKREL", rel->type(), 0, 0);
						if (_dag->root) {
							PWRN("LD: Unkown relocation %u", rel->type());
							throw Incompatible();
						}
						break;
				}
			}
		}
};

#endif /* _X86_32__RELOCATION_H_ */
