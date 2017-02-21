/*
 * \brief   Raspberry PI specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__RPI__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__RPI__BOARD_H_

#include <drivers/board_base.h>
#include <drivers/uart_base.h>
#include <hw/spec/arm/page_table.h>

#include <spec/arm/cpu.h>

namespace Bootstrap {
	using Serial   = Genode::Pl011_base;

	enum {
		UART_BASE  = Genode::Board_base::PL011_0_MMIO_BASE,
		UART_CLOCK = Genode::Board_base::PL011_0_CLOCK,
	};

	struct Pic {};
}


constexpr unsigned Hw::Page_table::Descriptor_base::_device_tex() {
	return 0; }


constexpr bool Hw::Page_table::Descriptor_base::_smp() { return false; }


void Hw::Page_table::_translation_added(unsigned long addr,
                                        unsigned long size) {
	Bootstrap::Cpu::clean_invalidate_data_cache(); }

#endif /* _SRC__BOOTSTRAP__SPEC__RPI__BOARD_H_ */
