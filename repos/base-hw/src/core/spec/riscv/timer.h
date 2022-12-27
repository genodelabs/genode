/*
 * \brief  Timer driver for core
 * \author Sebastian Sumpf
 * \date   2015-08-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__RISCV__TIMER_H_
#define _SRC__CORE__SPEC__RISCV__TIMER_H_

/* Genode includes */
#include <base/stdint.h>
#include <kernel/types.h>

namespace Board { class Timer; }


/**
 * Timer driver for core
 */
struct Board::Timer
{
	enum {
		TICKS_PER_MS = Hw::Riscv_board::TIMER_HZ / 1000,
		TICKS_PER_US = TICKS_PER_MS / 1000,
	};

	Kernel::time_t stime() const;

	Timer(unsigned);
};

#endif /* _SRC__CORE__SPEC__RISCV__TIMER_H_ */
