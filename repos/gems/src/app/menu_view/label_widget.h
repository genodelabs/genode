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
#include <text_selection.h>
#include <animated_color.h>

namespace Menu_view { struct Label_widget; }

struct Menu_view::Label_widget : Widget, Cursor::Glyph_position
{
	Text_painter::Font const *_font = nullptr;

	enum { LABEL_MAX_LEN = 256 };

	typedef String<200> Text;
	Text _text { };

	Animated_color _color;

	bool _hover = false;  /* report hover details */

	int _min_width  = 0;
	int _min_height = 0;

	List_model<Cursor>         _cursors    { };
	List_model<Text_selection> _selections { };

	Label_widget(Widget_factory &factory, Xml_node node, Unique_id unique_id)
	:
		Widget(factory, node, unique_id), _color(factory.animator)
	{ }

	~Label_widget() { _update_children(Xml_node("<empty/>")); }

	void _update_children(Xml_node const &node)
	{
		_cursors.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Cursor & {
				return *new (_factory.alloc)
					Cursor(node, _factory.animator, *this, _factory.styles); },

			/* destroy */
			[&] (Cursor &cursor) { destroy(_factory.alloc, &cursor); },

			/* update */
			[&] (Cursor &cursor, Xml_node const &node) {
				cursor.update(node); }
		);

		_selections.update_from_xml(node,

			/* create */
			[&] (Xml_node const &node) -> Text_selection & {
				return *new (_factory.alloc)
					Text_selection(node, *this); },

			/* destroy */
			[&] (Text_selection &t) { destroy(_factory.alloc, &t); },

			/* update */
			[&] (Text_selection &t, Xml_node const &node) {
				t.update(node); }
		);
	}

	void update(Xml_node node) override
	{
		_font       = _factory.styles.font(node);
		_text       = Text("");
		_min_width  = 0;
		_min_height = 0;
		_hover      = node.attribute_value("hover", false);

		_factory.styles.with_label_style(node, [&] (Label_style style) {
			_color.fade_to(style.color, Animated_color::Steps{80}); });

		if (node.has_attribute("text")) {
			_text       = node.attribute_value("text", _text);
			_text       = Xml_unquoted(_text);
			_min_height = _font ? _font->height() : 0;
		}

		unsigned const min_ex = node.attribute_value("min_ex", 0U);
		if (min_ex) {
			Glyph_painter::Fixpoint_number min_w_px = _font ? _font->string_width("x") : 0;
			min_w_px.value *= min_ex;
			_min_width = min_w_px.decimal();
		}

		_update_children(node);
	}

	Area min_size() const override
	{
		if (!_font)
			return Area(0, 0);

		return Area(max(_font->string_width(_text.string()).decimal(), _min_width),
		            _min_height);
	}

	void draw(Surface<Pixel_rgb888> &pixel_surface,
	          Surface<Pixel_alpha8> &alpha_surface,
	          Point at) const override
	{
		if (!_font) return;

		Area text_size = min_size();

		int const dx = (int)geometry().w() - text_size.w(),
		          dy = (int)geometry().h() - text_size.h();

		Point const centered = at + Point(dx/2, dy/2);

		_selections.for_each([&] (Text_selection const &selection) {
			selection.draw(pixel_surface, alpha_surface, at, text_size.h()); });

		Color const color = _color.color();
		int   const alpha = color.a;

		if (alpha) {
			Text_painter::paint(pixel_surface,
			                    Text_painter::Position(centered.x(), centered.y()),
			                    *_font, color, _text.string());

			Text_painter::paint(alpha_surface,
			                    Text_painter::Position(centered.x(), centered.y()),
			                    *_font, Color(alpha, alpha, alpha, alpha), _text.string());
		}

		_cursors.for_each([&] (Cursor const &cursor) {
			cursor.draw(pixel_surface, alpha_surface, at, text_size.h()); });
	}

	/**
	 * Cursor::Glyph_position interface
	 */
	int xpos_of_glyph(unsigned at) const override
	{
		return _font ? _font->string_width(_text.string(), at).decimal() : 0;
	}

	unsigned _char_index_at_xpos(unsigned xpos) const
	{
		return _font ? _font->index_at_xpos(_text.string(), xpos) : 0;
	}

	Hovered hovered(Point at) const override
	{
		if (!_hover)
			return Hovered { .unique_id = { }, .detail = { } };

		Unique_id const hovered_id = Widget::hovered(at).unique_id;

		if (!hovered_id.valid())
			return Hovered { .unique_id = hovered_id, .detail = { } };

		return { .unique_id = hovered_id,
		         .detail    = { _char_index_at_xpos(at.x()) } };
	}

	void gen_hover_model(Xml_generator &xml, Point at) const override
	{
		if (_inner_geometry().contains(at)) {

			xml.node(_type_name.string(), [&]() {

				_gen_common_hover_attr(xml);

				xml.attribute("at", _char_index_at_xpos(at.x()));
			});
		}
	}

	private:

		/**
		 * Noncopyable
		 */
		Label_widget(Label_widget const &);
		Label_widget &operator = (Label_widget const &);
};

#endif /* _LABEL_WIDGET_H_ */
