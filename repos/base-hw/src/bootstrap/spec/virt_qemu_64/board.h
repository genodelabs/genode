/*
 * \brief  Board driver for bootstrap
 * \author Piotr Tworek
 * \date   2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__VIRT_QEMU_64_H_
#define _SRC__BOOTSTRAP__SPEC__VIRT_QEMU_64_H_

#include <hw/spec/arm/virt_qemu_board.h>
#include <hw/spec/arm/gicv3.h>
#include <hw/spec/arm/lpae.h>
#include <hw/spec/arm_64/cpu.h>
#include <hw/spec/arm_64/psci_call.h>

namespace Board {
	using namespace Hw::Virt_qemu_board;

	using Psci = Hw::Psci<Hw::Psci_smc_functor>;

	struct Cpu : Hw::Arm_64_cpu
	{
		static void wake_up_all_cpus(void*);
	};

	using Hw::Pic;
};

#endif /* _SRC__BOOTSTRAP__SPEC__VIRT_QEMU_64_H_ */
