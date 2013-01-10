/*
 * \brief  Genode C API memory functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__MEMORY_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__MEMORY_H_

/**
 * Allocate memory from the heap
 *
 * \param sz amount of memory
 * \return   virtual address of the allocated memory area
 */
void *genode_malloc(unsigned long sz);

/**
 * Set the invoking thread as the pager of newly started Linux user processes
 */
void  genode_set_pager(void);

/**
 * Get the absolute maximum of RAM Linux is allowed to consume
 */
unsigned long genode_quota(void);

/**
 * Get the actual used portion of RAM
 */
unsigned long genode_used_mem(void);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__MEMORY_H_
