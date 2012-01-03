/*
 * \brief  Video memory allocator.
 * \author Stefan Kalkowski
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VIDEOMEM_ALLOCATOR_H_
#define _INCLUDE__VIDEOMEM_ALLOCATOR_H_

#include <base/stdint.h>
#include <dataspace/capability.h>

namespace Framebuffer {

	Genode::Dataspace_capability alloc_video_memory(Genode::size_t size);

}

#endif /* _INCLUDE__VIDEOMEM_ALLOCATOR_H_ */
