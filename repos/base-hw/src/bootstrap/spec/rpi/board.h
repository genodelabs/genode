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

#include <hw/spec/arm/rpi_board.h>
#include <hw/spec/arm/page_table.h>
#include <spec/arm/cpu.h>

namespace Board { using namespace Hw::Rpi_board; }


constexpr unsigned Hw::Page_table::Descriptor_base::_device_tex() {
	return 0; }


constexpr bool Hw::Page_table::Descriptor_base::_smp() { return false; }


void Hw::Page_table::_table_changed(unsigned long, unsigned long) { }

#endif /* _SRC__BOOTSTRAP__SPEC__RPI__BOARD_H_ */
