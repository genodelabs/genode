/*
 * \brief  Translation table definitions for core
 * \author Stefan Kalkowski
 * \date   2019-05-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V8__TRANSLATION_TABLE_H_
#define _CORE__SPEC__ARM_V8__TRANSLATION_TABLE_H_

/* core includes */
#include <hw/spec/arm/lpae.h>
#include <cpu.h>


template <unsigned BLOCK_SIZE_LOG2, unsigned SZ_LOG2>
void Hw::Long_translation_table<BLOCK_SIZE_LOG2, SZ_LOG2>::_update_cache(unsigned long addr, unsigned long size) {
    Genode::Cpu::cache_coherent_region(addr, size);
}

#endif /* _CORE__SPEC__ARM_V8__TRANSLATION_TABLE_H_ */
