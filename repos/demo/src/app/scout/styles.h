/*
 * \brief   Document styles
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STYLES_H_
#define _STYLES_H_

/* Genode includes */
#include <nitpicker_gfx/tff_font.h>

/* local includes */
#include "elements.h"

extern char _binary_mono16_tff_start[];
extern char _binary_verabi10_tff_start[];
extern char _binary_vera16_tff_start[];
extern char _binary_verai16_tff_start[];
extern char _binary_vera18_tff_start[];
extern char _binary_vera20_tff_start[];
extern char _binary_vera24_tff_start[];

namespace Scout {

	static Tff_font::Static_glyph_buffer<4096> _glyph_buffer { };

	static Tff_font label_font      { &_binary_verabi10_tff_start, _glyph_buffer };
	static Tff_font default_font    { &_binary_vera16_tff_start,   _glyph_buffer };
	static Tff_font italic_font     { &_binary_verai16_tff_start,  _glyph_buffer };
	static Tff_font mono_font       { &_binary_mono16_tff_start,   _glyph_buffer };
	static Tff_font chapter_font    { &_binary_vera24_tff_start,   _glyph_buffer };
	static Tff_font section_font    { &_binary_vera20_tff_start,   _glyph_buffer };
	static Tff_font subsection_font { &_binary_vera18_tff_start,   _glyph_buffer };

	static Tff_font &title_font = subsection_font;

	static Color default_color  { 0, 0, 0 };
	static Color text_color     { 20, 20, 20 };
	static Color verbatim_bgcol { 0, 0, 0, 26 };

	static Style plain_style      { &default_font, text_color, 0 };
	static Style bold_style       { &default_font, text_color, Style::ATTR_BOLD };
	static Style mono_style       { &mono_font,    text_color, 0 };
	static Style italic_style     { &italic_font,  text_color, 0 };

	static Style link_style       { &default_font, Color(0, 0, 255), 0 };

	static Style chapter_style    { &chapter_font,    default_color, 0 };
	static Style section_style    { &section_font,    default_color, 0 };
	static Style subsection_style { &subsection_font, default_color, 0 };
	static Style navbar_style     { &default_font,    Color(0, 0, 0, 127), 0 };
}

#endif /* _STYLES_H_ */
