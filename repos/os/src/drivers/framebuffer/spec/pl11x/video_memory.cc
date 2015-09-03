/*
 * \brief  PL11x video memory function for platform baseboard cortex-A9.
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

#include <pl11x_defs.h>
#include <video_memory.h>

Genode::Dataspace_capability Framebuffer::alloc_video_memory(Genode::size_t sz) {
	return Genode::env()->ram_session()->alloc(sz);
}
