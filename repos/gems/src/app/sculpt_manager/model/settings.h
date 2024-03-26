/*
 * \brief  Settings state
 * \author Norman Feske
 * \date   2020-01-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__SETTINGS_H_
#define _MODEL__SETTINGS_H_

#include "types.h"

namespace Sculpt { struct Settings; }


struct Sculpt::Settings
{
	enum class Font_size;

	enum class Font_size { SMALL, MEDIUM, LARGE };

	Font_size font_size = Font_size::MEDIUM;

	bool manual_fonts_config = false;

	struct Keyboard_layout
	{
		using Name = String<32>;

		Name name;
		Path chargen_file;

		static void for_each(auto const &fn)
		{
			static Keyboard_layout layouts[] = {
				{ .name = "French",       .chargen_file = "keyboard/fr_fr" },
				{ .name = "German",       .chargen_file = "keyboard/de_de" },
				{ .name = "Swiss French", .chargen_file = "keyboard/fr_ch" },
				{ .name = "Swiss German", .chargen_file = "keyboard/de_ch" },
				{ .name = "US English",   .chargen_file = "keyboard/en_us" },
			};

			for (auto layout : layouts)
				fn(layout);
		}
	};

	Keyboard_layout::Name keyboard_layout { "US English" };

	bool manual_event_filter_config = false;

	bool interactive_settings_available() const
	{
		return !manual_event_filter_config
		    || !manual_fonts_config;
	}
};

#endif /* _MODEL__SETTINGS_H_ */
