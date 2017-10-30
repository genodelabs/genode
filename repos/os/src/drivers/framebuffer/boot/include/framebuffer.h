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

#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <base/component.h>
#include <framebuffer_session/framebuffer_session.h>
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/signal.h>
#include <util/reconstructible.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_rgb888.h>

namespace Framebuffer {

	class Session_component;
}

class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Genode::Env &_env;

		struct fb_desc {
			Genode::uint64_t addr;
			Genode::uint32_t width;
			Genode::uint32_t height;
			Genode::uint32_t pitch;
			Genode::uint32_t bpp;
		} _core_fb;

		Mode _fb_mode;

		Genode::Constructible<Genode::Attached_io_mem_dataspace> _fb_mem;
		Genode::Constructible<Genode::Attached_ram_dataspace> _fb_ram;

		Timer::Connection timer { _env };

	public:
		Session_component(Genode::Env &, Genode::Xml_node);
		Mode mode() const override;
		void mode_sigh(Genode::Signal_context_capability) override;
		void sync_sigh(Genode::Signal_context_capability) override;
		void refresh(int, int, int, int) override;
		Genode::Dataspace_capability dataspace() override;
};

#endif // _FRAMEBUFFER_H_
