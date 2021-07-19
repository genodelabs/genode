/*
 * \brief  Replaces arch/arm64/kernel/smp.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <asm/smp.h>

int cpu_number = 0;
