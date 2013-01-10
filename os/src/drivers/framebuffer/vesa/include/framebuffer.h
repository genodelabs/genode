/*
 * \brief  Framebuffer driver interface, X86emu interface
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2006-09-11
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <base/capability.h>
#include <base/stdint.h>

#include "vbe.h"

namespace Framebuffer_drv {

	/**
	 * Return capability for h/w framebuffer dataspace
	 */
	Genode::Dataspace_capability hw_framebuffer();

	/**
	 * Initialize driver, x86emu lib, set up memory
	 */
	int init();

	/**
	 * Set video, initialize framebuffer dataspace
	 *
	 * \return  0 on success,
	 *          non-zero otherwise
	 */
	int set_mode(unsigned long width, unsigned long height, unsigned long mode);

	int use_current_mode();

	/**
	 * Map given device memory, return out_addr (map address)
	 */
	int map_io_mem(Genode::addr_t base, Genode::size_t size, bool write_combined,
	               void **out_addr, Genode::addr_t addr = 0,
	               Genode::Dataspace_capability *out_io = 0);
}

#endif /* _FRAMEBUFFER_H_ */
