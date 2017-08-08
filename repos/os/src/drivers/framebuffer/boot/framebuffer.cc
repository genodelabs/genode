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
	try {
		Genode::Xml_node fb = pinfo.sub_node("boot").sub_node("framebuffer");

		fb.attribute("phys").value(&_core_fb.addr);
		fb.attribute("width").value(&_core_fb.width);
		fb.attribute("height").value(&_core_fb.height);
		fb.attribute("bpp").value(&_core_fb.bpp);
	} catch (...) {
		Genode::error("No boot framebuffer information available.");
		throw Genode::Service_denied();
	}

	Genode::log("Framebuffer with ", _core_fb.width, "x", _core_fb.height,
	            "x", _core_fb.bpp, " @ ", (void*)_core_fb.addr);

	/* calculate required padding to align framebuffer to 16 pixels */
	_pad = (16 - (_core_fb.width % 16)) & 0x0f;

	_fb_mem.construct(
		_env,
		_core_fb.addr,
		(_core_fb.width + _pad) * _core_fb.height * _core_fb.bpp / 4,
		true);

	_fb_mode = Mode(_core_fb.width, _core_fb.height, Mode::RGB565);

	_fb_ram.construct(_env.ram(), _env.rm(),
	                  _core_fb.width * _core_fb.height * _fb_mode.bytes_per_pixel());
}

Mode Session_component::mode() const { return _fb_mode; }

void Session_component::mode_sigh(Genode::Signal_context_capability _scc) { }

void Session_component::sync_sigh(Genode::Signal_context_capability scc)
{
	timer.sigh(scc);
	timer.trigger_periodic(10*1000);
}

void Session_component::refresh(int x, int y, int w, int h)
{
	Genode::uint32_t u_x = (Genode::uint32_t)Genode::min(_core_fb.width,  (Genode::uint32_t)Genode::max(x, 0));
	Genode::uint32_t u_y = (Genode::uint32_t)Genode::min(_core_fb.height, (Genode::uint32_t)Genode::max(y, 0));
	Genode::uint32_t u_w = (Genode::uint32_t)Genode::min(_core_fb.width,  (Genode::uint32_t)Genode::max(w, 0) + u_x);
	Genode::uint32_t u_h = (Genode::uint32_t)Genode::min(_core_fb.height, (Genode::uint32_t)Genode::max(h, 0) + u_y);
	Genode::Pixel_rgb888 *pixel_32 = _fb_mem->local_addr<Genode::Pixel_rgb888>();
	Genode::Pixel_rgb565 *pixel_16 = _fb_ram->local_addr<Genode::Pixel_rgb565>();
	for (Genode::uint32_t r = u_y; r < u_h; ++r){
		for (Genode::uint32_t c = u_x; c < u_w; ++c){
			Genode::uint32_t s = c + r * _core_fb.width;
			Genode::uint32_t d = c + r * (_core_fb.width + _pad);
			pixel_32[d].rgba(
				pixel_16[s].r(),
				pixel_16[s].g(),
				pixel_16[s].b(),
				0);
		}
	}
}

Genode::Dataspace_capability Session_component::dataspace()
{
	return _fb_ram->cap();
}
