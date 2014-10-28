/*
 * \brief  Dummies for Codezeros libmem (used by libl4)
 * \author Sebastian Sumpf
 * \date   2011-05-10
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

void *mem_cache_zalloc(void *cache){ return 0; }
void *mem_cache_alloc(void *cache){ return 0; }
void *mem_cache_init(void *start, int cache_size, int struct_size,
                     unsigned int alignment) { return 0; }
int mem_cache_free(void *cache, void *addr) { return 0; }

void *kmalloc(int size) { return 0; }


