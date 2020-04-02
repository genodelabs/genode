/*
 * \brief  Board driver for bootstrap
 * \author Stefan Kalkowski
 * \date   2019-06-12
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BOOTSTRAP__SPEC__IMX8Q_EVK__BOARD_H_
#define _BOOTSTRAP__SPEC__IMX8Q_EVK__BOARD_H_

#include <hw/spec/arm_64/imx8q_evk_board.h>
#include <hw/spec/arm_64/cpu.h>
#include <hw/spec/arm/gicv3.h>
#include <hw/spec/arm/lpae.h>

namespace Board {
	using namespace Hw::Imx8q_evk_board;

	struct Cpu : Hw::Arm_64_cpu
	{
		static void wake_up_all_cpus(void*);
	};

	using Hw::Pic;
};

template <unsigned BLOCK_SIZE_LOG2, unsigned SZ_LOG2>
void Hw::Long_translation_table<BLOCK_SIZE_LOG2, SZ_LOG2>::_update_cache(unsigned long, unsigned long) { }

#endif /* _BOOTSTRAP__SPEC__IMX8Q_EVK__BOARD_H_ */
