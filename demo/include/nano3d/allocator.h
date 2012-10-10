/*
 * \brief  Allocator interface
 * \author Norman Feske
 * \date   2010-09-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NANO3D__ALLOCATOR_H_
#define _INCLUDE__NANO3D__ALLOCATOR_H_

namespace Nano3d {

	class Allocator
	{
		public:

			virtual void *alloc(unsigned long size) = 0;
			virtual void  free(void *ptr, unsigned long size) = 0;
	};
}

#endif /* _INCLUDE__NANO3D__ALLOCATOR_H_ */
