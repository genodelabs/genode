/*
 * \brief  Board driver definitions common to Cortex A9 SoCs
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__CORTEX_A9__BOARD_SUPPORT_H_
#define _CORE__INCLUDE__SPEC__CORTEX_A9__BOARD_SUPPORT_H_

/* core includes */
#include <drivers/board_base.h>
#include <spec/arm/pl310.h>

namespace Cortex_a9 { class Board; }

class Cortex_a9::Board : public Genode::Board_base
{
	public:

		using L2_cache = Arm::Pl310;

		static constexpr bool SMP = true;

		enum Errata {
			ARM_754322,
			ARM_764369,
			ARM_775420,
			PL310_588369,
			PL310_727915,
			PL310_769419,
		};

		enum {
			/* snoop control unit */
			SCU_MMIO_BASE             = CORTEX_A9_PRIVATE_MEM_BASE,

			/* interrupt controller */
			IRQ_CONTROLLER_DISTR_BASE = CORTEX_A9_PRIVATE_MEM_BASE + 0x1000,
			IRQ_CONTROLLER_DISTR_SIZE = 0x1000,
			IRQ_CONTROLLER_CPU_BASE   = CORTEX_A9_PRIVATE_MEM_BASE + 0x100,
			IRQ_CONTROLLER_CPU_SIZE   = 0x100,

			/* timer */
			PRIVATE_TIMER_MMIO_BASE   = CORTEX_A9_PRIVATE_MEM_BASE + 0x600,
			PRIVATE_TIMER_MMIO_SIZE   = 0x10,
			PRIVATE_TIMER_IRQ         = 29,
		};

		Board();

		L2_cache & l2_cache() { return _l2_cache; }

		void init() { }
		void wake_up_all_cpus(void * const ip);
		bool errata(Errata);

	protected:

		L2_cache _l2_cache;
};

#endif /* _CORE__INCLUDE__SPEC__CORTEX_A9__BOARD_SUPPORT_H_ */
