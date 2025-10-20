/*
 * \brief  Arm64 virt board driver for core
 * \author Piotr Tworek
 * \date   2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__VIRT_QEMU_64_H_
#define _SRC__CORE__SPEC__VIRT_QEMU_64_H_

/* base-hw internal includes */
#include <hw/spec/arm/virt_qemu_board.h>
#include <spec/arm/generic_timer.h>
#include <spec/arm/virtualization/gicv3.h>
#include <spec/arm_64/cpu/vcpu_state_virtualization.h>
#include <spec/arm/virtualization/board.h>
#include <kernel/configuration.h>
#include <kernel/irq.h>
#include <spec/arm_v8/cpu.h>

namespace Board {

	using namespace Hw::Virt_qemu_board;

	static constexpr Genode::size_t NR_OF_CPUS = 4;

	enum {
		TIMER_IRQ           = 30, /* PPI IRQ 14 */
		VT_TIMER_IRQ        = 11 + 16,
		VT_MAINTAINANCE_IRQ = 9  + 16,
		VCPU_MAX            = 16
	};
};

#endif /* _SRC__CORE__SPEC__VIRT_QEMU_64_H_ */
