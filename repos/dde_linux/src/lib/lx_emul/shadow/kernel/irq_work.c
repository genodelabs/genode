/*
 * \brief  Replaces kernel/irq_work.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/irq_work.h>


bool irq_work_needs_cpu(void)
{
	return false;
}
