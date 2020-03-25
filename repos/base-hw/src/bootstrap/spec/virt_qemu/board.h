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

#ifndef _SRC__BOOTSTRAP__SPEC__VIRT__QEMU_H_
#define _SRC__BOOTSTRAP__SPEC__VIRT__QEMU_H_

#include <hw/spec/arm/virt_qemu_board.h>
#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/lpae.h>
#include <hw/spec/arm/psci_call.h>
#include <spec/arm/cpu.h>

namespace Board {
	using namespace Hw::Virt_qemu_board;

	using Psci = Hw::Psci<Hw::Psci_smc_functor>;
	using Pic = Hw::Gicv2;
	static constexpr bool NON_SECURE = true;
};

#endif /* _SRC__BOOTSTRAP__SPEC__VIRT__QEMU_H_ */
