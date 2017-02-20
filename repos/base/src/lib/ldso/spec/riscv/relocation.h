/**
 * \brief  RISC-V 64  specific relocations
 * \author Sebastian Sumpf
 * \date   2015-09-07
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RISCV_64__RELOCATION_H_
#define _INCLUDE__RISCV_64__RELOCATION_H_

#include <relocation_generic.h>

namespace Linker {

	inline unsigned long dynamic_address()
	{
		unsigned long addr;
		asm volatile ("lla %0, _DYNAMIC" : "=r"(addr));
		return addr;
	}

	inline unsigned long relocation_address(void) { return 0; }

/**
 * Relocation types
 */
	enum Reloc_types {
		R_64       = 2, /* add 64 bit symbol valu with addend */
		R_RELATIVE = 3, /* add load addr of shared object     */
		R_JMPSLOT  = 5, /* jump slot                          */
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
			*addr = _dep.obj().reloc_base() + rel->addend;
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
				log("LD: GLOB DAT ", addr, " -> ", Hex(*addr),
				    " r ", Hex(reloc_base),
				    " v ", Hex(sym->st_value));
		}

	public:

		Reloc_non_plt(Dependency const &dep, Elf::Rela const *rel, unsigned long size, bool)
		: Reloc_non_plt_generic(dep)
		{
			Elf::Rela const *end = rel + (size / sizeof(Elf::Rela));

			for (; rel < end; rel++) {
				Elf::Addr *addr = (Elf::Addr *)(_dep.obj().reloc_base() + rel->offset);

				if (verbose_reloc(_dep))
					log("LD: reloc: ", rel, " type: ", (int)rel->type());

				switch(rel->type()) {
					case R_JMPSLOT:  _glob_dat_64(rel, addr, false); break;
					case R_64:       _glob_dat_64(rel, addr, true);  break;
					case R_RELATIVE: _relative(rel, addr);           break;

					default:
						if (!_dep.obj().is_linker()) {
							warning("LD: unkown relocation ", (int)rel->type());
							throw Incompatible();
						}
						break;
				}
			}
		}

		Reloc_non_plt(Dependency const &dep, Elf::Rel const *, unsigned long, bool)
		: Reloc_non_plt_generic(dep)
		{
			error("LD: DT_REL not supported");
			throw Incompatible();
		}
};

#endif /* _INCLUDE__RISCV_64__RELOCATION_H_ */
