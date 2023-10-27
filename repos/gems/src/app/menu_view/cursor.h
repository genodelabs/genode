/*
 * \brief  Text cursor
 * \author Norman Feske
 * \date   2020-01-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CURSOR_H_
#define _CURSOR_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <util/list_model.h>
#include <gems/animated_geometry.h>

/* local includes */
#include <types.h>
#include <widget_factory.h>

namespace Menu_view { struct Cursor; }

class Menu_view::Cursor : List_model<Cursor>::Element
{
	public:

		/**
		 * Interface for requesting the pixel position for a given char index
		 */
		struct Glyph_position : Interface, Noncopyable
		{
			virtual int xpos_of_glyph(unsigned at) const = 0;
		};

	private:

		friend class List_model<Cursor>;
		friend class List<Cursor>;

		using Steps = Animated_rect::Steps;

		Texture<Pixel_rgb888> const * const _texture;

		Glyph_position &_glyph_position;

		enum { NAME_MAX_LEN = 32 };
		typedef String<NAME_MAX_LEN> Name;

		Name const _name;

		/* cursor position in pixels, only p1.x is used */
		Animated_rect _position;

		int _xpos() const { return _position.p1().x(); }

		static Name _node_name(Xml_node node)
		{
			return node.attribute_value("name", Name(node.type()));
		}

		int _position_from_xml_node(Xml_node node)
		{
			return _glyph_position.xpos_of_glyph(node.attribute_value("at", 0U));
		}

		void _move_to(int position, Steps steps)
		{
			_position.move_to(Rect(Point(position, 0), Point()), steps);
		}

		/*
		 * Noncopyable
		 */
		Cursor(Cursor const &);
		void operator = (Cursor const &);

	public:

		Cursor(Xml_node node, Animator &animator, Glyph_position &glyph_position,
		       Style_database &styles)
		:
			_texture(styles.texture(node, "cursor")),
			_glyph_position(glyph_position),
			_name(_node_name(node)),
			_position(animator)
		{
			_move_to(_position_from_xml_node(node), Steps{0});
		}

		void draw(Surface<Pixel_rgb888> &pixel_surface,
		          Surface<Pixel_alpha8> &alpha_surface,
		          Point at, unsigned height) const
		{
			if (_texture == nullptr) {
				Box_painter::paint(pixel_surface,
				                   Rect(at + Point(_xpos(), 0), Area(1, height)),
				                   Color(0, 0, 0, 255));
			} else {
				unsigned const w = _texture->size().w();
				Rect const rect(Point(_xpos() + at.x() - w/2 + 1, at.y()),
				                Area(w, height));

				Icon_painter::paint(pixel_surface, rect, *_texture, 255);
				Icon_painter::paint(alpha_surface, rect, *_texture, 255);
			}
		}

		bool matches(Xml_node const &node) const
		{
			return _node_name(node) == _name;
		}

		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("cursor");
		}

		void update(Xml_node const &node)
		{
			_move_to(_position_from_xml_node(node), Steps{12});
		}
};

#endif /* _CURSOR_H_ */
