/**
 * \brief  ARM specific relocations
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM__RELOCATION_H_
#define _INCLUDE__ARM__RELOCATION_H_

#include <relocation_generic.h>

namespace Linker {
	enum Reloc_types {
		R_ABS32    = 2,
		R_REL32    = 3,
		R_COPY     = 20,
		R_GLOB_DAT = 21,
		R_JMPSLOT  = 22,
		R_RELATIVE = 23,
	};

	class Reloc_non_plt;

	typedef Reloc_plt_generic<Elf::Rel, DT_REL, R_JMPSLOT> Reloc_plt;
	typedef Reloc_jmpslot_generic<Elf::Rel, DT_REL, false> Reloc_jmpslot;
	typedef Reloc_bind_now_generic<Elf::Rel, DT_REL>       Reloc_bind_now;
};


class Linker::Reloc_non_plt : public Reloc_non_plt_generic
{
	private:

		void _rel32(Elf::Rel const *rel, Elf::Addr *addr)
		{
			Elf::Addr reloc_base;
			Elf::Sym  const *sym;

			if (!(sym = locate_symbol(rel->sym(), _dag, &reloc_base)))
				return;

			/* S + A - P */
			*addr = reloc_base + sym->st_value - (Elf::Addr)addr + *addr;
			trace("REL32", (unsigned long)addr, *addr, 0);
		}

		void _glob_dat(Elf::Rel const *rel, Elf::Addr *addr, bool no_addend)
		{
			Elf::Addr reloc_base;
			Elf::Sym  const *sym;

			if (!(sym = locate_symbol(rel->sym(), _dag, &reloc_base)))
				return;

			Elf::Addr addend = no_addend ? 0 : *addr;

			/* S + A */
			*addr = addend + reloc_base + sym->st_value;
			trace("GLOB_DAT", (unsigned long)addr, *addr, 0);
		}

		void _relative(Elf::Addr *addr)
		{
			/*
			 * This ommits the linker and the binary, the linker has relative
			 * relocations within its text-segment (e.g., 'initial_sp' and friends), which
			 * we cannot write to from here).
			 */
			if (_dag->obj->reloc_base())
				*addr += _dag->obj->reloc_base();
		}

	public:

		Reloc_non_plt(Dag const *dag, Elf::Rela const *, unsigned long)
		: Reloc_non_plt_generic(dag)
		{
			PERR("LD: DT_RELA not supported");
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

					case R_REL32   : _rel32(rel, addr);                 break;
					case R_COPY    : _copy<Elf::Rel>(rel, addr);        break;
					case R_ABS32   :
					case R_GLOB_DAT: _glob_dat(rel, addr, second_pass); break;
					case R_RELATIVE: _relative(addr);                   break;
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

#endif /* _INCLUDE__ARM__RELOCATION_H_ */
