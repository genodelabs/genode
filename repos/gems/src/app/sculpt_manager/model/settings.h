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

		template <typename FN>
		static void for_each(FN const &fn)
		{
			static Keyboard_layout layouts[] = {
				{ .name = "French",       .chargen_file = "fr_fr.chargen" },
				{ .name = "German",       .chargen_file = "de_de.chargen" },
				{ .name = "Swiss French", .chargen_file = "fr_ch.chargen" },
				{ .name = "Swiss German", .chargen_file = "de_ch.chargen" },
				{ .name = "US English",   .chargen_file = "en_us.chargen" },
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
