/*
 * \brief   Armv6 translation table for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_V6__TRANSLATION_TABLE_H_
#define _CORE__SPEC__ARM_V6__TRANSLATION_TABLE_H_

#include <hw/spec/arm/page_table.h>
#include <spec/arm/cpu_support.h>
#include <kernel/interface.h>

constexpr unsigned Hw::Page_table::Descriptor_base::_device_tex() {
	return 0; }

constexpr bool Hw::Page_table::Descriptor_base::_smp() { return false; }

void Hw::Page_table::_table_changed(unsigned long addr, unsigned long size)
{
	Genode::Arm_cpu::clean_data_cache_by_virt_region(addr, size);
}

#endif /* _CORE__SPEC__ARM_V6__TRANSLATION_TABLE_H_ */
