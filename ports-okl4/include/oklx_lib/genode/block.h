/*
 * \brief  Genode C API block driver related functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2010-07-08
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__BLOCK_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__BLOCK_H_

void  genode_block_register_callback(void (*func)(void*, short,
                                                  void*, unsigned long));

void  genode_block_geometry(unsigned long *blk_cnt, unsigned long *blk_sz,
                            int *writeable, unsigned long *req_queue_sz);

void* genode_block_request(unsigned long sz, void *req, unsigned long *offset);

void  genode_block_submit(unsigned long queue_offset, unsigned long size,
                          unsigned long disc_offset, int write);

void  genode_block_collect_responses(void);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__BLOCK_H_
