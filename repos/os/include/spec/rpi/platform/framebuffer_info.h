/*
 * \brief  Framebuffer info structure
 * \author Norman Feske
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM__FRAMEBUFFER_INFO_H_
#define _PLATFORM__FRAMEBUFFER_INFO_H_

#include <base/stdint.h>

namespace Platform {
	using namespace Genode;
	struct Framebuffer_info;
}


/**
 * Structure used by the protocol between the Videocore GPU and the ARM CPU for
 * setting up the framebuffer via the mbox.
 */
struct Platform::Framebuffer_info
{
	uint32_t phys_width;
	uint32_t phys_height;
	uint32_t virt_width;
	uint32_t virt_height;
	uint32_t pitch = 0;
	uint32_t depth;
	uint32_t x_offset = 0;
	uint32_t y_offset = 0;
	uint32_t addr = 0;
	uint32_t size = 0;

	/**
	 * Default constructor needed to make the object transferable via RPC
	 */
	Framebuffer_info()
	:
		phys_width(0), phys_height(0), virt_width(0), virt_height(),
		depth(0)
	{ }

	Framebuffer_info(uint32_t width, uint32_t height, uint32_t depth)
	:
		phys_width(width), phys_height(height),
		virt_width(width), virt_height(height),
		depth(depth)
	{ }
};

#endif /* _PLATFORM__FRAMEBUFFER_INFO_H_ */
