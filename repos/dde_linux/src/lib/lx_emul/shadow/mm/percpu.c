/*
 * \brief  Replaces mm/percpu.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/percpu.h>
#include <lx_emul/alloc.h>

void __percpu * __alloc_percpu(size_t size, size_t align)
{
	return lx_emul_mem_alloc_aligned(size, align);
}


void __percpu * __alloc_percpu_gfp(size_t size,size_t align,gfp_t gfp)
{
	return __alloc_percpu(size, align);
}


void free_percpu(void __percpu * ptr)
{
	lx_emul_mem_free(ptr);
}
