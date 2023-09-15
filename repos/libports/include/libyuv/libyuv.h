/*
 * \brief  Libyuv initialization to overwrite default libc memory/free allocator
 * \author Alexander Boettcher
 * \date   2023-09-15
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBYUV__LIBYUV_H_
#define _INCLUDE__LIBYUV__LIBYUV_H_

/* use same namespace as used in contrib libyuv sources */
namespace libyuv
{
	typedef void * (*type_malloc)(unsigned long); 
	typedef void   (*type_free  )(void *);

	extern "C" void libyuv_init(type_malloc os_malloc, type_free os_free);
}

#endif /* _INCLUDE__LIBYUV_LIBYUV_H_ */
