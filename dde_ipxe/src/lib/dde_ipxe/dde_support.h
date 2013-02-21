/*
 * \brief  DDE iPXE wrappers to C++ backend
 * \author Christian Helmuth
 * \date   2013-01-07
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DDE_SUPPORT_H_
#define _DDE_SUPPORT_H_

#include <dde_kit/types.h>


void *dde_alloc_memblock(dde_kit_size_t size, dde_kit_size_t align,
                         dde_kit_size_t offset);
void dde_free_memblock(void *p, dde_kit_size_t size);
void dde_timer2_udelay(unsigned long usecs);
int  dde_mem_init(int bus, int dev, int func);

#endif /* _DDE_SUPPORT_H_ */
