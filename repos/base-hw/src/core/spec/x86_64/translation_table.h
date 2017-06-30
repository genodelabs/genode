/*
 * \brief  x86_64 translation table definitions for core
 * \author Adrian-Ken Rueegsegger
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__TRANSLATION_TABLE_H_
#define _CORE__SPEC__X86_64__TRANSLATION_TABLE_H_

#include <hw/spec/x86_64/page_table.h>
#include <cpu.h>

void Hw::Pml4_table::_invalidate_range(addr_t vo, size_t size)
{
	/* FIXME: do not necessarily flush the whole TLB */
	Genode::Cpu::Cr3::write(Genode::Cpu::Cr3::read());

}

#endif /* _CORE__SPEC__X86_64__TRANSLATION_TABLE_H_ */
