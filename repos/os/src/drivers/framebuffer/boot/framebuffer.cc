/*
 * \brief  Framebuffer driver that uses a framebuffer supplied by the core rom.
 * \author Johannes Kliemann
 * \date   2017-06-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <framebuffer.h>
#include <base/component.h>

using namespace Framebuffer;

Session_component::Session_component(Genode::Env &env,
                                     Genode::Xml_node pinfo)
: _env(env)
{
	enum { RGB_COLOR = 1 };

	unsigned fb_boot_type = 0;

	try {
		Genode::Xml_node fb = pinfo.sub_node("boot").sub_node("framebuffer");

		fb.attribute("phys").value(&_core_fb.addr);
		fb.attribute("width").value(&_core_fb.width);
		fb.attribute("height").value(&_core_fb.height);
		fb.attribute("bpp").value(&_core_fb.bpp);
		fb.attribute("pitch").value(&_core_fb.pitch);
		fb_boot_type = fb.attribute_value("type", 0U);
	} catch (...) {
		Genode::error("No boot framebuffer information available.");
		throw Genode::Service_denied();
	}

	Genode::log("Framebuffer with ", _core_fb.width, "x", _core_fb.height,
	            "x", _core_fb.bpp, " @ ", (void*)_core_fb.addr,
	            " type=", fb_boot_type, " pitch=", _core_fb.pitch);

	if (_core_fb.bpp != 32 || fb_boot_type != RGB_COLOR ) {
		Genode::error("unsupported resolution (bpp or/and type)");
		throw Genode::Service_denied();
	}

	_fb_mem.construct(_env, _core_fb.addr, _core_fb.pitch * _core_fb.height,
	                  true);

	_fb_mode = Mode(_core_fb.width, _core_fb.height, Mode::RGB565);

	_fb_ram.construct(_env.ram(), _env.rm(), _core_fb.width * _core_fb.height *
	                                         _fb_mode.bytes_per_pixel());
}

Mode Session_component::mode() const { return _fb_mode; }

void Session_component::mode_sigh(Genode::Signal_context_capability) { }

void Session_component::sync_sigh(Genode::Signal_context_capability scc)
{
	timer.sigh(scc);
	timer.trigger_periodic(10*1000);
}

void Session_component::refresh(int const x, int const y, int const w, int const h)
{
	using namespace Genode;

	uint32_t const c_x = x < 0 ? 0U : x;
	uint32_t const c_y = y < 0 ? 0U : y;
	uint32_t const c_w = w < 0 ? 0U : w;
	uint32_t const c_h = h < 0 ? 0U : h;

	uint32_t const u_x = min(_core_fb.width,  max(c_x, 0U));
	uint32_t const u_y = min(_core_fb.height, max(c_y, 0U));
	uint32_t const u_w = min(_core_fb.width,  max(c_w, 0U) + u_x);
	uint32_t const u_h = min(_core_fb.height, max(c_h, 0U) + u_y);

	Pixel_rgb888       * const pixel_32 = _fb_mem->local_addr<Pixel_rgb888>();
	Pixel_rgb565 const * const pixel_16 = _fb_ram->local_addr<Pixel_rgb565>();

	for (uint32_t r = u_y; r < u_h; ++r) {
		for (uint32_t c = u_x; c < u_w; ++c) {
			uint32_t const s = c + r * _core_fb.width;
			uint32_t const d = c + r * (_core_fb.pitch / (_core_fb.bpp / 8));

			pixel_32[d].rgba(pixel_16[s].r(), pixel_16[s].g(),
			                 pixel_16[s].b(), 0);
		}
	}
}

Genode::Dataspace_capability Session_component::dataspace()
{
	return _fb_ram->cap();
}
