/*
 * \brief  RISCV Sv39 page table format
 * \author Sebastian Sumpf
 * \date   2015-08-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RISCV__TRANSLATION_TABLE_H_
#define _CORE__SPEC__RISCV__TRANSLATION_TABLE_H_

#include <hw/spec/riscv/page_table.h>
#include <kernel/interface.h>

template <typename E, unsigned B, unsigned S>
void Sv39::Level_x_translation_table<E, B, S>::_translation_added(addr_t addr,
                                                                  size_t size)
{
	Kernel::update_data_region(addr, size);
}

#endif /* _CORE__SPEC__RISCV__TRANSLATION_TABLE_H_ */
