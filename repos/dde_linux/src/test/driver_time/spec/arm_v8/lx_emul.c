/*
 * \brief  Implementation of Linux specific functions
 * \author Sebastian Sumpf
 * \date   2024-07-01
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <clocksource/arm_arch_timer.h>

extern unsigned long long lx_emul_timestamp(void);

u64 (*arch_timer_read_counter)(void) = lx_emul_timestamp;
