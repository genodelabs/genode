/*
 * \brief  Supplement for emulation of kernel/sched/core.c
 * \author Josef Soentgen
 * \date   2022-05-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <lx_emul.h>

#include <linux/sched.h>


void sched_set_fifo(struct task_struct * p)
{
    lx_emul_trace(__func__);
}
