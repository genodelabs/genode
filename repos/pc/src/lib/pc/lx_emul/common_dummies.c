/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Josef Soentgen
 * \date   2022-02-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <asm-generic/sections.h>

char __start_rodata[] = {};
char __end_rodata[]   = {};

#include <asm/preempt.h>

int __preempt_count = 0;

#include <linux/bitops.h>

unsigned int __sw_hweight32(__u32 w)
{
	lx_emul_trace_and_stop(__func__);
}

#include <linux/bitops.h>

unsigned long __sw_hweight64(__u64 w)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/prandom.h>

unsigned long net_rand_noise;


#include <linux/tracepoint-defs.h>

const struct trace_print_flags gfpflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags vmaflag_names[]  = { {0,NULL}};


#include <linux/tracepoint-defs.h>

const struct trace_print_flags pageflag_names[] = { {0,NULL}};


#include <linux/kernel_stat.h>

struct kernel_stat kstat;
