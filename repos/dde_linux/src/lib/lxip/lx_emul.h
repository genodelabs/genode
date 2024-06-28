/*
 * \brief  Definitions of Linux Kernel functions
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/debug.h>

#include <linux/init.h>
#include <linux/sched/debug.h>


/* kernel/sched/sched.h */
struct affinity_context;
