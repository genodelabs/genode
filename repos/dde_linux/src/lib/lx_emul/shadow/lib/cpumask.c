/*
 * \brief  Replaces lib/cpumask.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/cpumask.h>
#include <lx_emul/debug.h>

unsigned int cpumask_next(int n,const struct cpumask * srcp)
{
	return n + 1;
}


int cpumask_next_and(int                    n,
                     const struct cpumask * src1p,
                     const struct cpumask * src2p)
{
	return n + 1;
}


