/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Alexander Boettcher
 * \date   2022-07-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/debug.h>


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/random.h>

u32 __get_random_u32_below(u32 ceil)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

DEFINE_STATIC_KEY_FALSE(force_irqthreads_key);


#include <net/net_namespace.h>

void net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/skbuff.h>

void skb_init()
{
	lx_emul_trace(__func__);
}
