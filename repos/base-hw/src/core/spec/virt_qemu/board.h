/*
 * \brief  Arm virt board driver for core
 * \author Piotr Tworek
 * \date   2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__VIRT__QEMU_H_
#define _SRC__CORE__SPEC__VIRT__QEMU_H_

#include <hw/spec/arm/virt_qemu_board.h>
#include <hw/spec/arm/gicv2.h>
#include <spec/arm/generic_timer.h>

namespace Board {
	using namespace Hw::Virt_qemu_board;
	using Pic = Hw::Gicv2;

	enum { TIMER_IRQ = 30 /* PPI IRQ 14 */ };
};

#endif /* _SRC__CORE__SPEC__VIRT__QEMU_H_ */
