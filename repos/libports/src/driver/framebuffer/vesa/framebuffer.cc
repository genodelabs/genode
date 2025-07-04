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


static void for_each_mode(mb_vbe_ctrl_t const &ctrl_info, auto const &fn)
{
	/*
	 * The virtual address of the ctrl_info mapping may change on x86_cmd
	 * execution. Therefore, we resolve the address on each iteration.
	 */
#define MODE_PTR(off) (X86emu::virt_addr<uint16_t>(to_phys(ctrl_info.video_mode)) + off)
	for (unsigned off = 0; *MODE_PTR(off) != 0xFFFF; ++off) {
		if (X86emu::x86emu_cmd(VBE_INFO_FUNC, 0, *MODE_PTR(off), VESA_MODE_OFFS) != VBE_SUPPORTED)
			continue;

		fn(*MODE_PTR(off));

	}
#undef MODE_PTR
}


static uint16_t get_vesa_mode(mb_vbe_ctrl_t const &ctrl_info,
                              mb_vbe_mode_t const &mode_info,
                              Capture::Area &phys, Capture::Area &virt,
                              unsigned const depth, bool const verbose)
{
	bool choose_highest_resolution_mode = !virt.valid();

	uint16_t ret = 0;

	if (verbose)
		log("Supported mode list");

	auto const &fn = [&](auto const &mode) {

		enum { DIRECT_COLOR = 0x06 };
		if (mode_info.memory_model != DIRECT_COLOR)
			return;

		if (verbose)
			log("    ", Hex((short)mode, Hex::PREFIX, Hex::PAD), " ",
			    (unsigned)mode_info.x_resolution, "x",
			    (unsigned)mode_info.y_resolution, "@",
			    (unsigned)mode_info.bits_per_pixel);

		if (mode_info.bits_per_pixel != depth)
			return;

		if (choose_highest_resolution_mode) {
			if (!((mode_info.x_resolution > virt.w) ||
			     ((mode_info.x_resolution == virt.w) &&
			      (mode_info.y_resolution > virt.h))))
				return;
		} else {
			if (mode_info.x_resolution != virt.w || mode_info.y_resolution != virt.h)
				return;
		}

		phys.w = mode_info.bytes_per_scanline / (mode_info.bits_per_pixel / 8);
		phys.h = mode_info.y_resolution;

		virt.w = mode_info.x_resolution;
		virt.h = mode_info.y_resolution;

		ret = mode;
	};

	for_each_mode(ctrl_info, fn);

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
		if (ret != 0)
			virt = { 1024, 768 };

		return ret;
	}

	return get_default_vesa_mode(virt.w, virt.h, depth);
}


static void generate_report(Xml_generator       &xml,
                            mb_vbe_ctrl_t const &ctrl_info,
                            mb_vbe_mode_t const &mode_info,
                            unsigned      const  depth,
                            uint16_t      const  vesa_mode)
{
	xml.node("merge", [&]() {
		xml.attribute("name", "mirror");

		xml.node("connector", [&] () {
			xml.attribute("connected", true);
			xml.attribute("name", "VESA");

			for_each_mode(ctrl_info, [&](auto const &mode) {

				enum { DIRECT_COLOR = 0x06 };
				if (mode_info.memory_model != DIRECT_COLOR)
					return;

				if (mode_info.bits_per_pixel != depth)
					return;

				auto name = String<32>(mode_info.x_resolution, "x",
				                       mode_info.y_resolution);

				xml.node("mode", [&] () {
					xml.attribute(    "id", mode);
					xml.attribute( "width", mode_info.x_resolution);
					xml.attribute("height", mode_info.y_resolution);
					xml.attribute(  "name", name);
					if (mode == vesa_mode)
						xml.attribute("used", true);
				});
			});
		});
	});
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

	genode_env().rm().attach(io_ds, {
		.size       = size,
		.offset     = { },
		.use_at     = (addr != 0),
		.at         = addr,
		.executable = false,
		.writeable  = true
	}).with_result(
		[&] (Env::Local_rm::Attachment &a) {
			a.deallocate = false;
			*out_addr = a.ptr; },
		[&] (Env::Local_rm::Error) { }
	);

	if (*out_addr == nullptr)
		return -3;

	log("fb mapped to ", *out_addr);

	if (out_io_ds)
		*out_io_ds = io_ds;

	return 0;
}


int Framebuffer::set_mode(Expanding_reporter &reporter,
                          Capture::Area &phys, Capture::Area &virt,
                          unsigned const depth)
{
	/* set location of data types */
	auto &ctrl_info = *reinterpret_cast<mb_vbe_ctrl_t*>(X86emu::x86_mem.data_addr()
	                                                    + VESA_CTRL_OFFS);
	auto &mode_info = *reinterpret_cast<mb_vbe_mode_t*>(X86emu::x86_mem.data_addr()
	                                                    + VESA_MODE_OFFS);

	/* request VBE 2.0 information */
	memcpy(ctrl_info.signature, "VBE2", 4);

	/* retrieve controller information */
	if (X86emu::x86emu_cmd(VBE_CONTROL_FUNC, 0, 0, VESA_CTRL_OFFS) != VBE_SUPPORTED) {
		warning("VBE Bios not present");
		return -1;
	}

	/* retrieve vesa mode hex value */
	auto const vesa_mode = get_vesa_mode(ctrl_info, mode_info, phys, virt,
	                                     depth, verbose);
	if (!vesa_mode) {
		warning("graphics mode ", virt, "@", depth, " not found");
		/* print available modes */
		get_vesa_mode(ctrl_info, mode_info, phys, virt, depth, true);
		return -2;
	}

	/* use current refresh rate, set flat framebuffer model */
	auto const vesa_mode_cmd = (vesa_mode & VBE_CUR_REFRESH_MASK) | VBE_SET_FLAT_FB;

	/* determine VBE version and OEM string */
	auto oem_string = X86emu::virt_addr<char>(to_phys(ctrl_info.oem_string));

	log("Found: VESA BIOS version ",
	    ctrl_info.version >> 8, ".", ctrl_info.version & 0xFF, "\n"
	    "OEM: ", Cstring(ctrl_info.oem_string ? oem_string : "[unknown]"));

	if (ctrl_info.version < 0x200) {
		warning("VESA Bios version 2.0 or later required");
		return -3;
	}

	/* get mode info */
	/* 0x91 tests MODE SUPPORTED (0x1) | GRAPHICS  MODE (0x10) | LINEAR
	 * FRAME BUFFER (0x80) bits */
	if (X86emu::x86emu_cmd(VBE_INFO_FUNC, 0, vesa_mode_cmd, VESA_MODE_OFFS) != VBE_SUPPORTED
	   || (mode_info.mode_attributes & 0x91) != 0x91) {
		warning("graphics mode ", virt, "@", depth, " not supported");
		/* print available modes */
		get_vesa_mode(ctrl_info, mode_info, phys, virt, depth, true);
		return -4;
	}

	/* set mode */
	if ((X86emu::x86emu_cmd(VBE_MODE_FUNC, vesa_mode_cmd) & 0xFF00) != VBE_SUCCESS) {
		error("VBE SET error");
		return -5;
	}

	/* map framebuffer */
	void *fb;
	if (!io_mem_cap.valid()) {
		X86emu::x86emu_cmd(VBE_INFO_FUNC, 0, vesa_mode_cmd, VESA_MODE_OFFS);

		log("Found: physical frame buffer at ", Hex(mode_info.phys_base), " "
		    "size: ", ctrl_info.total_memory << 16);
		map_io_mem(mode_info.phys_base, ctrl_info.total_memory << 16, true,
		           &fb, 0, &io_mem_cap);
	}

	if (verbose)
		X86emu::print_regions();

	reporter.generate([&] (auto &xml) {
		generate_report(xml, ctrl_info, mode_info, depth, vesa_mode); });

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
