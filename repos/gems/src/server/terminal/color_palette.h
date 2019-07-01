/*
 * \brief  Terminal color palette
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COLOR_PALETTE_H_
#define _COLOR_PALETTE_H_

/* Genode includes */
#include <util/color.h>
#include <util/xml_node.h>

/* local includes */
#include "types.h"

namespace Terminal {
	class Color_palette;
}


class Terminal::Color_palette
{
	public:

		struct Index { unsigned value; };

		struct Highlighted { bool value; };

	private:

		enum { NUM_COLORS = 16U };

		Color _colors[NUM_COLORS];

		static constexpr char const * _default_palette {
			"<palette>"
			" <color index=\"0\"  value=\"#000000\"/>" /* black */
			" <color index=\"1\"  value=\"#AC4142\"/>" /* red */
			" <color index=\"2\"  value=\"#90A959\"/>" /* green */
			" <color index=\"3\"  value=\"#F4BF75\"/>" /* yellow */
			" <color index=\"4\"  value=\"#7686BD\"/>" /* blue */
			" <color index=\"5\"  value=\"#AA759F\"/>" /* magenta */
			" <color index=\"6\"  value=\"#75B5AA\"/>" /* cyan */
			" <color index=\"7\"  value=\"#D0D0D0\"/>" /* white */
			" <color index=\"8\"  value=\"#101010\"/>" /* bright black */
			" <color index=\"9\"  value=\"#AC4142\"/>" /* bright red */
			" <color index=\"10\" value=\"#90A959\"/>" /* bright green */
			" <color index=\"11\" value=\"#F4BF75\"/>" /* bright yellow */
			" <color index=\"12\" value=\"#6A9FB5\"/>" /* bright blue */
			" <color index=\"13\" value=\"#AA759F\"/>" /* bright magenta */
			" <color index=\"14\" value=\"#75B5AA\"/>" /* bright cyan */
			" <color index=\"15\" value=\"#F5F5F5\"/>" /* bright white */
			"</palette>" };

		Genode::Xml_node const _default { _default_palette };

		void _apply_palette(Xml_node palette)
		{
			palette.for_each_sub_node("color", [&] (Xml_node node) {
				if (!node.has_attribute("index")) return;
				if (!node.has_attribute("value")) return;

				unsigned const index = node.attribute_value("index", 0U);
				if (!(index <= NUM_COLORS)) return;
				_colors[index] = node.attribute_value("value", Color());
			});
		}

	public:

		Color_palette()
		{
			_apply_palette(_default);
		}

		void apply_config(Xml_node config)
		{
			_apply_palette(_default);
			if (config.has_sub_node("palette"))
				_apply_palette(config.sub_node("palette"));
		}

		Color foreground(Index index, Highlighted highlighted) const
		{
			if (index.value >= NUM_COLORS/2)
				return Color(0, 0, 0);

			Color const col =
				_colors[index.value + (highlighted.value ? NUM_COLORS/2 : 0)];

			return col;
		}

		Color background(Index index, Highlighted highlighted) const
		{
			Color const color = foreground(index, highlighted);

			/* reduce the intensity of background colors */
			return Color(color.r*3/4, color.g*3/4, color.b*3/4);
		}
};

#endif /* _COLOR_PALETTE_H_ */
