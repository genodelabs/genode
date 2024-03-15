/*
 * \brief  Boot-time framebuffer information
 * \author Norman Feske
 * \date   2024-03-15
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__BOOT_FB_H_
#define _MODEL__BOOT_FB_H_

#include <types.h>

namespace Sculpt { struct Boot_fb; }


struct Sculpt::Boot_fb
{
	struct Mode
	{
		unsigned pitch, height;

		static Mode from_xml(Xml_node const &framebuffer)
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

	static void with_mode(Xml_node const &platform, auto const &fn)
	{
		platform.with_optional_sub_node("boot", [&] (Xml_node const &boot) {
			boot.with_optional_sub_node("framebuffer", [&] (Xml_node const &framebuffer) {
				fn(Mode::from_xml(framebuffer)); }); });
	}
};

#endif /* _MODEL__BOOT_FB_H_ */
