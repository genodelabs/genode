/*
 * \brief  Replaces mm/slub.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/mm.h>
#include <linux/slab.h>
#include <../mm/slab.h>
#include <lx_emul/alloc.h>
#include <lx_emul/debug.h>

void * krealloc(const void * p,size_t new_size,gfp_t flags)
{
	if (!p)
		return kmalloc(new_size, flags);

	if (!new_size) {
		kfree(p);
		return NULL;

	} else {

		unsigned long const old_size = ksize(p);
		void *ret;

		if (new_size <= old_size)
			return (void*) p;

		ret = kmalloc(new_size, flags);
		memcpy(ret, p, old_size);
		return ret;
	}
}


size_t ksize(const void * objp)
{
	if (objp == NULL)
		return 0;

	return __ksize(objp);
}
