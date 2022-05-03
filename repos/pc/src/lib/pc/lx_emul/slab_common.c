/*
 * \brief  Supplement for emulation of mm/slab_common.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/slab.h>

void * kmalloc_order(size_t size, gfp_t flags, unsigned int order)
{
	return kmalloc(size, flags);
}


