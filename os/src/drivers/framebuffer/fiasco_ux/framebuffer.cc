/**
 * \brief  Fiasco-UX Framebuffer driver
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/stdint.h>
#include <base/printf.h>
#include <base/snprintf.h>

#include <rom_session/connection.h>
#include <io_mem_session/connection.h>

namespace Fiasco {
#include <l4/sys/vhw.h>
}

#include "framebuffer.h"

using namespace Genode;


/**
 * Framebuffer area
 */
static Dataspace_capability io_mem_cap;


/****************
 ** Driver API **
 ****************/

Dataspace_capability Framebuffer_drv::hw_framebuffer()
{
	return io_mem_cap;
}


/********************
 ** Driver startup **
 ********************/

/**
 * Configure Fiasco kernel info page
 */
static void *map_kip()
{
	/* request KIP dataspace */
	Rom_connection rom("l4v2_kip");
	rom.on_destruction(Rom_connection::KEEP_OPEN);

	/* attach KIP dataspace */
	return env()->rm_session()->attach(rom.dataspace());
}


/**
 * Read virtual hardware descriptor from kernel info page
 */
static int init_framebuffer_vhw(void *kip, addr_t *base, size_t *size)
{
	Fiasco::l4_kernel_info_t *kip_ptr = (Fiasco::l4_kernel_info_t *)kip;
	struct Fiasco::l4_vhw_descriptor *vhw = Fiasco::l4_vhw_get(kip_ptr);
	if (!vhw) return -1;

	struct Fiasco::l4_vhw_entry *e = Fiasco::l4_vhw_get_entry_type(vhw, Fiasco::L4_TYPE_VHW_FRAMEBUFFER);
	if (!e) return -2;

	*base = e->mem_start;
	*size = e->mem_size;

	return 0;
}


/**
 * Configure io_mem area containing Fiasco-UX framebuffer
 */
Dataspace_capability map_framebuffer_area(addr_t base, size_t size, void **framebuffer)
{
	/* request io_mem dataspace */
	Io_mem_connection io_mem(base, size);
	io_mem.on_destruction(Io_mem_connection::KEEP_OPEN);
	Io_mem_dataspace_capability io_mem_ds = io_mem.dataspace();
	if (!io_mem_ds.valid()) return Dataspace_capability();

	/* attach io_mem dataspace */
	*framebuffer = env()->rm_session()->attach(io_mem_ds);
	return io_mem_ds;
}


int Framebuffer_drv::init()
{
	using namespace Genode;

	void *kip = 0;
	try { kip = map_kip(); }
	catch (...) {
		PERR("KIP mapping failed");
		return 1;
	}

	addr_t base; size_t size;
	if (init_framebuffer_vhw(kip, &base, &size)) {
		PERR("VHW framebuffer init failed");
		return 2;
	}

	PDBG("--- framebuffer area is [%lx,%lx) ---", base, base + size);

	void *framebuffer = 0;
	io_mem_cap = map_framebuffer_area(base, size, &framebuffer);
	if (!io_mem_cap.valid()) {
		PERR("VHW framebuffer area mapping failed");
		return 3;
	}

	return 0;
}

