/*
 * \brief  PL11x video memory function for versatile express A9x4.
 * \author Stefan Kalkowski
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <io_mem_session/connection.h>

#include <pl11x_defs.h>
#include <video_memory.h>

Genode::Dataspace_capability Framebuffer::alloc_video_memory(Genode::size_t sz) {
	using namespace Genode;

	Io_mem_connection *fb_mem =
		new (env()->heap()) Io_mem_connection(PL11X_VIDEO_RAM, sz);
	return fb_mem->dataspace();
}
