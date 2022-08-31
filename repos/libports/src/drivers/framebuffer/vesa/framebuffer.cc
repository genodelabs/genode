/*
 * \brief  VESA frame buffer driver back end
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/allocator.h>
#include <io_mem_session/connection.h>

/* local includes */
#include "framebuffer.h"
#include "ifx86emu.h"
#include "hw_emul.h"
#include "vesa.h"
#include "vbe.h"
#include "genode_env.h"

using namespace Genode;
using namespace Vesa;

/**
 * Frame buffer I/O memory dataspace
 */
static Dataspace_capability io_mem_cap;

static const bool verbose = false;


/***************
 ** Utilities **
 ***************/


static inline uint32_t to_phys(uint32_t addr)
{
	return (addr & 0xFFFF) + ((addr >> 12) & 0xFFFF0);
}


static uint16_t get_vesa_mode(mb_vbe_ctrl_t *ctrl_info, mb_vbe_mode_t *mode_info,
                              unsigned &width, unsigned &height, unsigned depth,
                              bool verbose)
{
	bool choose_highest_resolution_mode = ((width == 0) || (height == 0));

	uint16_t ret = 0;

	if (verbose)
		log("Supported mode list");

	/*
	 * The virtual address of the ctrl_info mapping may change on x86_cmd
	 * execution. Therefore, we resolve the address on each iteration.
	 */
#define MODE_PTR(off) (X86emu::virt_addr<uint16_t>(to_phys(ctrl_info->video_mode)) + off)
	for (unsigned off = 0; *MODE_PTR(off) != 0xFFFF; ++off) {
		if (X86emu::x86emu_cmd(VBE_INFO_FUNC, 0, *MODE_PTR(off), VESA_MODE_OFFS) != VBE_SUPPORTED)
			continue;

		enum { DIRECT_COLOR = 0x06 };
		if (mode_info->memory_model != DIRECT_COLOR)
			continue;

		if (verbose)
			log("    ", Hex((short)*MODE_PTR(off), Hex::PREFIX, Hex::PAD), " ",
			    (unsigned)mode_info->x_resolution, "x",
			    (unsigned)mode_info->y_resolution, "@",
			    (unsigned)mode_info->bits_per_pixel);

		if (choose_highest_resolution_mode) {
			if ((mode_info->bits_per_pixel == depth) &&
			    ((mode_info->x_resolution > width) ||
			     ((mode_info->x_resolution == width) &&
			      (mode_info->y_resolution > height)))) {
				/*
				 * FIXME
				 *
				 * The width of a line in the framebuffer can be higher than
				 * the visible width (for example: visible width 1366,
				 * framebuffer width 1376). Currently, the framebuffer width
				 * is reported to the client, which does not know the
				 * difference and assumes the whole width to be completely
				 * visible.
				 */
				width = mode_info->bytes_per_scanline / (mode_info->bits_per_pixel / 8);
				height = mode_info->y_resolution;
				ret = *MODE_PTR(off);
			}
		} else {
			if (mode_info->x_resolution == width &&
			    mode_info->y_resolution == height &&
			    mode_info->bits_per_pixel == depth) {
				/*
				 * FIXME
				 *
				 * The width of a line in the framebuffer can be higher than
				 * the visible width (for example: visible width 1366,
				 * framebuffer width 1376). Currently, the framebuffer width
				 * is reported to the client, which does not know the
				 * difference and assumes the whole width to be completely
				 * visible.
				 */
				width = mode_info->bytes_per_scanline / (mode_info->bits_per_pixel / 8);
				ret = *MODE_PTR(off);
			}
		}
	}
#undef MODE_PTR

	if (ret)
		return ret;

	if (verbose)
		warning("Searching in default vesa modes");

	if (choose_highest_resolution_mode) {
		/*
		 * We did not find any mode for the given color depth so far.
		 * Default to 1024x768 for now.
		 */
		ret = get_default_vesa_mode(1024, 768, depth);
		if (ret != 0) {
			width = 1024;
			height = 768;
		}
		return ret;
	}

	return get_default_vesa_mode(width, height, depth);
}


/****************
 ** Driver API **
 ****************/

Dataspace_capability Framebuffer::hw_framebuffer()
{
	return io_mem_cap;
}


int Framebuffer::map_io_mem(addr_t base, size_t size, bool write_combined,
                            void **out_addr, addr_t addr,
                            Dataspace_capability *out_io_ds)
{
	Io_mem_connection &io_mem = *new (alloc())
		Io_mem_connection(genode_env(), base, size, write_combined);

	Io_mem_dataspace_capability io_ds = io_mem.dataspace();
	if (!io_ds.valid())
		return -2;

	try {
		*out_addr = genode_env().rm().attach(io_ds, size, 0, addr != 0, addr);
	}
	catch (Region_map::Region_conflict) { return -3; }

	log("fb mapped to ", *out_addr);

	if (out_io_ds)
		*out_io_ds = io_ds;

	return 0;
}


int Framebuffer::set_mode(unsigned &width, unsigned &height, unsigned mode)
{
	mb_vbe_ctrl_t *ctrl_info;
	mb_vbe_mode_t *mode_info;
	char * oem_string;
	uint16_t vesa_mode;

	/* set location of data types */
	ctrl_info = reinterpret_cast<mb_vbe_ctrl_t*>(X86emu::x86_mem.data_addr()
	                                             + VESA_CTRL_OFFS);
	mode_info = reinterpret_cast<mb_vbe_mode_t*>(X86emu::x86_mem.data_addr()
	                                             + VESA_MODE_OFFS);

	/* request VBE 2.0 information */
	memcpy(ctrl_info->signature, "VBE2", 4);

	/* retrieve controller information */
	if (X86emu::x86emu_cmd(VBE_CONTROL_FUNC, 0, 0, VESA_CTRL_OFFS) != VBE_SUPPORTED) {
		warning("VBE Bios not present");
		return -1;
	}

	/* retrieve vesa mode hex value */
	if (!(vesa_mode = get_vesa_mode(ctrl_info, mode_info, width, height, mode, verbose))) {
		warning("graphics mode ", width, "x", height, "@", mode, " not found");
		/* print available modes */
		get_vesa_mode(ctrl_info, mode_info, width, height, mode, true);
		return -2;
	}

	/* use current refresh rate, set flat framebuffer model */
	vesa_mode = (vesa_mode & VBE_CUR_REFRESH_MASK) | VBE_SET_FLAT_FB;

	/* determine VBE version and OEM string */
	oem_string = X86emu::virt_addr<char>(to_phys(ctrl_info->oem_string));

	log("Found: VESA BIOS version ",
	    ctrl_info->version >> 8, ".", ctrl_info->version & 0xFF, "\n"
	    "OEM: ", Cstring(ctrl_info->oem_string ? oem_string : "[unknown]"));

	if (ctrl_info->version < 0x200) {
		warning("VESA Bios version 2.0 or later required");
		return -3;
	}

	/* get mode info */
	/* 0x91 tests MODE SUPPORTED (0x1) | GRAPHICS  MODE (0x10) | LINEAR
	 * FRAME BUFFER (0x80) bits */
	if (X86emu::x86emu_cmd(VBE_INFO_FUNC, 0, vesa_mode, VESA_MODE_OFFS) != VBE_SUPPORTED
	   || (mode_info->mode_attributes & 0x91) != 0x91) {
		warning("graphics mode ", width, "x", height, "@", mode, " not supported");
		/* print available modes */
		get_vesa_mode(ctrl_info, mode_info, width, height, mode, true);
		return -4;
	}

	/* set mode */
	if ((X86emu::x86emu_cmd(VBE_MODE_FUNC, vesa_mode) & 0xFF00) != VBE_SUCCESS) {
		error("VBE SET error");
		return -5;
	}

	/* map framebuffer */
	void *fb;
	if (!io_mem_cap.valid()) {
		X86emu::x86emu_cmd(VBE_INFO_FUNC, 0, vesa_mode, VESA_MODE_OFFS);

		log("Found: physical frame buffer at ", Hex(mode_info->phys_base), " "
		    "size: ", ctrl_info->total_memory << 16);
		map_io_mem(mode_info->phys_base, ctrl_info->total_memory << 16, true,
		           &fb, 0, &io_mem_cap);
	}

	if (verbose)
		X86emu::print_regions();

	return 0;
}


/********************
 ** Driver startup **
 ********************/

void Framebuffer::init(Genode::Env &env, Genode::Allocator &heap)
{
	local_init_genode_env(env, heap);
	hw_emul_init(env);
	X86emu::init(env, heap);
}
