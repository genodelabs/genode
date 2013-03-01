/*
 * \brief  Genode C API block driver related functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2010-07-08
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__BLOCK_H_
#define _INCLUDE__GENODE__BLOCK_H_

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

#include <genode/linkage.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV unsigned genode_block_count(void);

L4_CV const char *genode_block_name(unsigned idx);

L4_CV l4_cap_idx_t genode_block_irq_cap(unsigned idx);

L4_CV void genode_block_register_callback(FASTCALL void (*func)(void*, short,
                                          void*, unsigned long));

L4_CV void genode_block_geometry(unsigned idx, unsigned long *blk_cnt,
                                 unsigned long *blk_sz, int *writeable,
                                 unsigned long *req_queue_sz);

L4_CV void* genode_block_request(unsigned idx, unsigned long sz,
                                 void *req, unsigned long *offset);

L4_CV void genode_block_submit(unsigned idx, unsigned long queue_offset,
                               unsigned long size, unsigned long long disc_offset,
                               int write);

L4_CV void genode_block_collect_responses(unsigned idx);

#ifdef __cplusplus
}
#endif

#endif //_INCLUDE__GENODE__BLOCK_H_
