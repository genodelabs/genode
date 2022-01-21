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

void lx_emul_time_udelay(unsigned long usec);

/* shadow/asm/io.h */
void lx_emul_io_port_outb(unsigned char  value, unsigned short port);
void lx_emul_io_port_outw(unsigned short value, unsigned short port);
void lx_emul_io_port_outl(unsigned int   value, unsigned short port);

unsigned char  lx_emul_io_port_inb(unsigned short port);
unsigned short lx_emul_io_port_inw(unsigned short port);
unsigned int   lx_emul_io_port_inl(unsigned short port);

void *emul_alloc_shmem_file_buffer(unsigned long);


#include "lx_i915.h"

#ifdef __cplusplus
}
#endif
