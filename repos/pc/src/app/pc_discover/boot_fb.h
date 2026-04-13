/*
 * \brief  Boot-time framebuffer information
 * \author Norman Feske
 * \date   2026-04-13
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BOOT_FB_H_
#define _BOOT_FB_H_

#include <types.h>

namespace Pc_discover { struct Boot_fb; }


struct Pc_discover::Boot_fb
{
	struct Mode
	{
		unsigned pitch, height;

		static Mode from_node(Node const &framebuffer)
		{
			static constexpr unsigned TYPE_RGB_COLOR = 1;

			if (framebuffer.attribute_value("type", 0U) != TYPE_RGB_COLOR)
				return { };

			return {
				.pitch  = framebuffer.attribute_value("pitch",  0U),
				.height = framebuffer.attribute_value("height", 0U)
			};
		}

		Ram_quota ram_quota() const { return { pitch*height + 1024*1024 }; }

		bool valid() const { return pitch*height != 0; }
	};

	static void with_mode(Node const &platform, auto const &fn)
	{
		platform.with_optional_sub_node("boot", [&] (Node const &boot) {
			boot.with_optional_sub_node("framebuffer", [&] (Node const &framebuffer) {
				fn(Mode::from_node(framebuffer)); }); });
	}
};

#endif /* _BOOT_FB_H_ */
