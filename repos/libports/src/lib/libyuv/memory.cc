/*
 * \brief  Support to overwrite default memory allocator of libyuv
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 2.
 */

#include <base/log.h>

#include <stdlib.h>


typedef void * (*type_malloc)(unsigned long);
typedef void   (*type_free  )(void *);


static type_malloc use_malloc(type_malloc m = malloc)
{
	static auto defined_malloc = m;
	return defined_malloc;
}


static type_free use_free(type_free f = free)
{
	static auto defined_free = f;
	return defined_free;
}


extern "C" void libyuv_init(type_malloc os_malloc, type_free os_free)
{
	if (!os_malloc || !os_free) {
		Genode::error("invalid libyuv allocator specified");
		return;
	}

	use_malloc(os_malloc);
	use_free  (os_free);
}


extern "C" void *libyuv_malloc(unsigned long size)
{
	return use_malloc()(size);
}


extern "C" void  libyuv_free(void *ptr)
{
	use_free()(ptr);
}
