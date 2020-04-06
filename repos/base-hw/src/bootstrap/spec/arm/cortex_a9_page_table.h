/*
 * \brief   Cortex A9 specific page table format
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_PAGE_TABLE_H_
#define _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_PAGE_TABLE_H_

#include <hw/spec/arm/page_table.h>

constexpr unsigned Hw::Page_table::Descriptor_base::_device_tex() {
	return 2; }

constexpr bool Hw::Page_table::Descriptor_base::_smp() { return true; }

void Hw::Page_table::_table_changed(unsigned long, unsigned long) { }

#endif /* _SRC__BOOTSTRAP__SPEC__ARM__CORTEX_A9_PAGE_TABLE_H_ */
