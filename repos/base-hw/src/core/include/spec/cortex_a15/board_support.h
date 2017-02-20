/*
 * \brief  Board driver definitions common to Cortex A15 SoCs
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__CORTEX_A15__BOARD_SUPPORT_H_
#define _CORE__INCLUDE__SPEC__CORTEX_A15__BOARD_SUPPORT_H_

/* core includes */
#include <drivers/board_base.h>

namespace Cortex_a15
{
	class Board_base : public Genode::Board_base
	{
		private:

			using Base = Genode::Board_base;

		public:

			enum
			{
				/* interrupt controller */
				IRQ_CONTROLLER_DISTR_BASE = Base::IRQ_CONTROLLER_BASE + 0x1000,
				IRQ_CONTROLLER_DISTR_SIZE = 0x1000,
				IRQ_CONTROLLER_CPU_BASE   = Base::IRQ_CONTROLLER_BASE + 0x2000,
				IRQ_CONTROLLER_CPU_SIZE   = 0x2000,
			};
	};
}

#endif /* _CORE__INCLUDE__SPEC__CORTEX_A15__BOARD_SUPPORT_H_ */
