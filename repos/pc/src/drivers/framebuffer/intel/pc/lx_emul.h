/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


/* Needed to trace and stop */
#include <lx_emul/debug.h>

#ifdef __cplusplus
extern "C" {
#endif


/* fix for wait_for_completion_timeout where the __sched include is missing */
#include <linux/sched/debug.h>

struct dma_fence_work;
struct dma_fence_work_ops;

void * intel_io_mem_map(unsigned long offset, unsigned long size);

#include "lx_i915.h"

unsigned short emul_intel_gmch_control_reg(void);

enum { OPREGION_PSEUDO_PHYS_ADDR = 0xffffefff };

unsigned long long emul_avail_ram(void);

#ifdef __cplusplus
}
#endif
