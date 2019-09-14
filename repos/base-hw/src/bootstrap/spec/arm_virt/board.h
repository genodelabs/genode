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

#pragma once

#include <hw/spec/arm/arm_virt_board.h>
#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/lpae.h>
#include <hw/spec/arm/psci.h>
#include <spec/arm/cpu.h>

namespace Board {
	using namespace Hw::Arm_virt_board;

	using Psci = Hw::Psci<Hw::Psci_conduit::HVC, Hw::PSCI_32>;
	using Pic = Hw::Gicv2;
	static constexpr bool NON_SECURE = false;
};
