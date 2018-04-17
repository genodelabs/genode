/*
 * \brief  Widget that displays a single line of plain text
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LABEL_WIDGET_H_
#define _LABEL_WIDGET_H_

/* local includes */
#include <widget_factory.h>

namespace Menu_view { struct Label_widget; }

struct Menu_view::Label_widget : Widget
{
	Text_painter::Font const *font = nullptr;

	enum { LABEL_MAX_LEN = 256 };

	typedef String<200> Text;
	Text text;

	Label_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id)
	{ }

	void update(Xml_node node)
	{
		font = _factory.styles.font(node);
		text = Decorator::string_attribute(node, "text", Text(""));
	}

	Area min_size() const override
	{
		if (!font)
			return Area(0, 0);

		return Area(font->string_width(text.string()).decimal(),
		            font->height());
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const
	{
		if (!font) return;

		Area text_size = min_size();

		int const dx = (int)geometry().w() - text_size.w(),
		          dy = (int)geometry().h() - text_size.h();

		Point const centered = at + Point(dx/2, dy/2);

		Text_painter::paint(pixel_surface,
		                    Text_painter::Position(centered.x(), centered.y()),
		                    *font, Color(0, 0, 0), text.string());

		Text_painter::paint(alpha_surface,
		                    Text_painter::Position(centered.x(), centered.y()),
		                    *font, Color(255, 255, 255), text.string());
	}
};

#endif /* _LABEL_WIDGET_H_ */
