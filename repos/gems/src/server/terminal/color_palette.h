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

namespace Terminal { class Color_palette; }


class Terminal::Color_palette
{
	public:

		struct Index { unsigned value; };

		struct Highlighted { bool value; };

	private:

		enum { NUM_COLORS = 16U };

		Color _colors[NUM_COLORS];

		static constexpr char const * _default_palette[NUM_COLORS] {
			"#000000", /*  0  black          */
			"#AC4142", /*  1  red            */
			"#90A959", /*  2  green          */
			"#F4BF75", /*  3  yellow         */
			"#7686BD", /*  4  blue           */
			"#AA759F", /*  5  magenta        */
			"#75B5AA", /*  6  cyan           */
			"#D0D0D0", /*  7  white          */
			"#101010", /*  8  bright black   */
			"#AC4142", /*  9  bright red     */
			"#90A959", /* 10  bright green   */
			"#F4BF75", /* 11  bright yellow  */
			"#6A9FB5", /* 12  bright blue    */
			"#AA759F", /* 13  bright magenta */
			"#75B5AA", /* 14  bright cyan    */
			"#F5F5F5", /* 15  bright white   */
		};

		void _apply_default()
		{
			for (unsigned i = 0; i < NUM_COLORS; i++)
				ascii_to(_default_palette[i], _colors[i]);
		}

		void _apply_palette(Node const &palette)
		{
			palette.for_each_sub_node("color", [&] (Node const &node) {
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
			_apply_default();
		}

		void apply_config(Node const &config)
		{
			_apply_default();
			config.with_optional_sub_node("palette", [&] (Node const &palette) {
				_apply_palette(palette); });
		}

		Color foreground(Index index, Highlighted highlighted) const
		{
			if (index.value >= NUM_COLORS/2)
				return Color::black();

			Color const col =
				_colors[index.value + (highlighted.value ? NUM_COLORS/2 : 0)];

			return col;
		}

		Color background(Index index, Highlighted highlighted) const
		{
			Color const color = foreground(index, highlighted);

			/* reduce the intensity of background colors */
			return Color::clamped_rgb(color.r*3/4, color.g*3/4, color.b*3/4);
		}
};

#endif /* _COLOR_PALETTE_H_ */
