/**
 * \brief  ARM 64-bit specific relocations
 * \author Stefan Kalkowski
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__LDSO__SPEC__ARM_64__RELOCATION_H_
#define _LIB__LDSO__SPEC__ARM_64__RELOCATION_H_

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

		void _relative(Elf::Rela const *, Elf::Addr *)
		{
			error("not implemented yet");
		}

		void _glob_dat_64(Elf::Rela const *, Elf::Addr *, bool)
		{
			error("not implemented yet");
		}

	public:

		Reloc_non_plt(Dependency const &dep, Elf::Rela const *, unsigned long, bool)
		: Reloc_non_plt_generic(dep)
		{
			error("not implemented yet");
		}

		Reloc_non_plt(Dependency const &dep, Elf::Rel const *, unsigned long, bool)
		: Reloc_non_plt_generic(dep)
		{
			error("not implemented yet");
		}
};

#endif /* _LIB__LDSO__SPEC__ARM_64__RELOCATION_H_ */
