/*
 * \brief  x86_64 constants
 * \author Reto Buerki
 * \date   2015-03-18
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* base-hw internal includes */
#include <hw/spec/x86_64/pc_board.h>
/* PC virtualization */
#include <spec/x86_64/virtualization/board.h>

/* base-hw core includes */
#include <spec/x86_64/pic.h>
#include <spec/x86_64/timer.h>
#include <spec/x86_64/cpu.h>

namespace Board {

	using namespace Hw::Pc_board;

	class Pic : public Local_interrupt_controller
	{ using Local_interrupt_controller::Local_interrupt_controller; };

	enum {
		VECTOR_REMAP_BASE   = 48,
		TIMER_VECTOR_KERNEL = 32,
		TIMER_VECTOR_USER   = 50,
		ISA_IRQ_END         = 15,
	};
}

#endif /* _BOARD_H_ */
