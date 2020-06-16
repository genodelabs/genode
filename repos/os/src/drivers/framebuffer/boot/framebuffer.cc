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
#include <blit/blit.h>

using namespace Framebuffer;

Session_component::Session_component(Genode::Env &env,
                                     Genode::Xml_node pinfo)
: _env(env)
{
	enum { RGB_COLOR = 1 };

	unsigned fb_boot_type = 0;

	try {
		Genode::Xml_node fb = pinfo.sub_node("boot").sub_node("framebuffer");

		fb.attribute("phys").value(_core_fb.addr);
		fb.attribute("width").value(_core_fb.width);
		fb.attribute("height").value(_core_fb.height);
		fb.attribute("bpp").value(_core_fb.bpp);
		fb.attribute("pitch").value(_core_fb.pitch);
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

	_fb_mode = Mode { .area = { _core_fb.width, _core_fb.height } };

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

	int      const width  = _core_fb.width;
	int      const height = _core_fb.height;
	unsigned const bpp    = 4;
	unsigned const pitch  = _core_fb.pitch;

	/* clip specified coordinates against screen boundaries */
	int const x2 = min(x + w - 1, width  - 1),
	          y2 = min(y + h - 1, height - 1);
	int const x1 = max(x, 0),
	          y1 = max(y, 0);
	if (x1 > x2 || y1 > y2)
		return;

	/* copy pixels from back buffer to physical frame buffer */
	char const *src = _fb_ram->local_addr<char>() + bpp*width*y1 + bpp*x1;
	char       *dst = _fb_mem->local_addr<char>() + pitch*y1     + bpp*x1;

	blit(src, bpp*width, dst, pitch, bpp*(x2 - x1 + 1), y2 - y1 + 1);
}

Genode::Dataspace_capability Session_component::dataspace()
{
	return _fb_ram->cap();
}
