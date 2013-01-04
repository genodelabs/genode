/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <util/lazy_value.h>
#include <decorator/window.h>

/* local includes */
#include <animator.h>


namespace Decorator { class Window; }


class Decorator::Window : public Window_base
{
	public:

		typedef Genode::String<200> Title;

	private:

		Title _title;

		bool _focused = false;

		Animator &_animator;

		static unsigned const _corner_size = 16;
		static unsigned const _border_size = 4;
		static unsigned const _title_height = 16;

		static Border _border() {
			return Border(_border_size + _title_height,
			              _border_size, _border_size, _border_size); }

		Color _bright = { 255, 255, 255, 64 };
		Color _dark   = { 0, 0, 0, 127 };

		Color _base_color() const { return Color(45, 49, 65); }

		class Element : public Animator::Item
		{
			public:

				enum Type { TITLE, LEFT, RIGHT, TOP, BOTTOM,
				            TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
				            UNDEFINED };

			private:

				static Color _add(Color c1, Color c2)
				{
					return Color(Genode::min(c1.r + c2.r, 255),
					             Genode::min(c1.g + c2.g, 255),
					             Genode::min(c1.b + c2.b, 255));
				}

				Type _type;

				/*
				 * Color value in 8.4 fixpoint format. We use four bits to
				 * represent the fractional part to enable smooth
				 * interpolation between the color values.
				 */
				Lazy_value<unsigned> _r, _g, _b;

				bool _focused     = false;
				bool _highlighted = false;

				static Color _dst_color(bool focused, bool highlighted, Color base)
				{
					Color result = base;

					if (focused)
						result = _add(result, Color(70, 70, 70));

					if (highlighted)
						result = _add(result, Color(65, 60, 55));

					return result;
				}

				unsigned _anim_steps(bool focused, bool highlighted) const
				{
					/* quick fade-in when gaining the focus or hover highlight */
					if ((!_focused && focused) || (!_highlighted && highlighted))
						return 20;

					/* slow fade-out when leaving focus or hover highlight */
					return 180;
				}

				bool _apply_state(bool focused, bool highlighted, Color base_color)
				{
					Color const dst_color = _dst_color(focused, highlighted, base_color);
					unsigned const steps = _anim_steps(focused, highlighted);

					_r.dst(dst_color.r << 4, steps);
					_g.dst(dst_color.g << 4, steps);
					_b.dst(dst_color.b << 4, steps);

					/* schedule animation */
					animate();

					_focused     = focused;
					_highlighted = highlighted;

					return true;
				}

			public:

				Element(Type type, Animator &animator, Color base_color)
				:
					Animator::Item(animator),
					_type(type)
				{
					_apply_state(false, false, base_color);
				}

				Type type() const { return _type; }

				char const *type_name() const
				{
					switch (_type) {
					case UNDEFINED:    return "";
					case TITLE:        return "title";
					case LEFT:         return "left";
					case RIGHT:        return "right";
					case TOP:          return "top";
					case BOTTOM:       return "bottom";
					case TOP_LEFT:     return "top_left";
					case TOP_RIGHT:    return "top_right";
					case BOTTOM_LEFT:  return "bottom_left";
					case BOTTOM_RIGHT: return "bottom_right";
					}
					return "";
				}

				Color color() const { return Color(_r >> 4, _g >> 4, _b >> 4); }

				/**
				 * \return true if state has changed
				 */
				bool apply_state(bool focused, bool highlighted, Color base_color)
				{
					if (_focused == focused && _highlighted == highlighted)
						return false;

					return _apply_state(focused, highlighted, base_color);
				}

				/**
				 * Animator::Item interface
				 */
				void animate() override;
		};

		/*
		 * The element order must correspond to the order of enum values
		 * because the type is used as index into the '_elements' array.
		 */
		Element _elements[9] { { Element::TITLE,        _animator, _base_color() },
		                       { Element::LEFT,         _animator, _base_color() },
		                       { Element::RIGHT,        _animator, _base_color() },
		                       { Element::TOP,          _animator, _base_color() },
		                       { Element::BOTTOM,       _animator, _base_color() },
		                       { Element::TOP_LEFT,     _animator, _base_color() },
		                       { Element::TOP_RIGHT,    _animator, _base_color() },
		                       { Element::BOTTOM_LEFT,  _animator, _base_color() },
		                       { Element::BOTTOM_RIGHT, _animator, _base_color() } };

		Element &element(Element::Type type)
		{
			return _elements[type];
		}

		Element const &element(Element::Type type) const
		{
			return _elements[type];
		}

		unsigned num_elements() const { return sizeof(_elements)/sizeof(Element); }

		void _draw_hline(Canvas_base &canvas, Point pos, unsigned w,
		                 bool at_left, bool at_right,
		                 unsigned border, Color color) const
		{
			int const x1 = at_left  ? (pos.x())         : (pos.x() + w - border);
			int const x2 = at_right ? (pos.x() + w - 1) : (pos.x() + border - 1);

			canvas.draw_box(Rect(Point(x1, pos.y()),
			                     Point(x2, pos.y())), color);
		}

		void _draw_vline(Canvas_base &canvas, Point pos, unsigned h,
		                 bool at_top, bool at_bottom,
		                 unsigned border, Color color) const
		{
			int const y1 = at_top    ? (pos.y())         : (pos.y() + h - border);
			int const y2 = at_bottom ? (pos.y() + h - 1) : (pos.y() + border - 1);

			canvas.draw_box(Rect(Point(pos.x(), y1),
			                     Point(pos.x(), y2)), color);
		}

		void _draw_raised_frame(Canvas_base &canvas, Rect rect) const
		{
			_draw_hline(canvas, rect.p1(), rect.w(), true, true, 0, _bright);
			_draw_vline(canvas, rect.p1(), rect.h(), true, true, 0, _bright);
			_draw_hline(canvas, Point(rect.p1().x(), rect.p2().y()), rect.w(),
			            true, true, 0, _dark);
			_draw_vline(canvas, Point(rect.p2().x(), rect.p1().y()), rect.h(),
			            true, true, 0, _dark);
		}

		void _draw_raised_box(Canvas_base &canvas, Rect rect, Color color) const
		{
			canvas.draw_box(rect, color);
			_draw_raised_frame(canvas, rect);
		}

		void _draw_title_box(Canvas_base &canvas, Rect rect, Color color) const
		{
			canvas.draw_box(rect, color);
			for (unsigned i = 0; i < rect.h(); i++)
				canvas.draw_box(Rect(rect.p1() + Point(0, i),
				                     Area(rect.w(), 1)),
				                     Color(255,255,255, 30 + (rect.h() - i)*4));

			_draw_raised_frame(canvas, rect);
		}

		void _draw_corner(Canvas_base &canvas, Rect const rect,
		                  unsigned const border,
		                  bool const left, bool const top,
		                  Color color) const
		{
			bool const bottom = !top;
			bool const right  = !left;

			int const x1 = rect.p1().x();
			int const y1 = rect.p1().y();
			int const x2 = rect.p2().x();
			int const y2 = rect.p2().y();
			int const w  = rect.w();
			int const h  = rect.h();

			canvas.draw_box(Rect(Point(x1, top ? y1 : y2 - border + 1),
			                     Area(w, border)), color);

			canvas.draw_box(Rect(Point(left ? x1 : x2 - border + 1,
			                           top  ? y1 + border : y1),
			                     Area(border, h - border)), color);

			/* top bright line */
			_draw_hline(canvas, rect.p1(), w,
			            top || left, top || right, border, _bright);

			/* inner horizontal line */
			int y = top ? y1 + border - 1 : y2 - border + 1;
			_draw_hline(canvas, Point(x1, y), w, right, left, w - border,
			            top ? _dark : _bright);

			/* bottom line */
			_draw_hline(canvas, Point(x1, y2), w,
			            bottom || left, bottom || right, border, _dark);

			/* left bright line */
			_draw_vline(canvas, rect.p1(), h,
			            left || top, left || bottom, border, _bright);

			/* inner vertical line */
			int x = left ? x1 + border - 1 : x2 - border + 1;
			_draw_vline(canvas, Point(x, y1), h, bottom, top, h - border + 1,
			            left ? _dark : _bright);

			/* right line */
			_draw_vline(canvas, Point(x2, y1), h,
			            right || top, right || bottom, border, _dark);
		}

		bool _apply_state(Window::Element::Type type, bool focused, bool highlighted)
		{
			return element(type).apply_state(_focused, highlighted, _base_color());
		}

	public:

		Window(unsigned id, Nitpicker::Session_client &nitpicker, Animator &animator)
		:
			Window_base(id, nitpicker, _border()),
			_animator(animator)
		{ }

		void draw(Canvas_base &canvas, Rect clip) const override;

		bool update(Xml_node window_node) override;

		Hover hover(Point) const override;

		bool animated() const override
		{
			for (unsigned i = 0; i < num_elements(); i++)
				if (_elements[i].animated())
					return true;

			return false;
		}
};


void Decorator::Window::draw(Decorator::Canvas_base &canvas,
                             Decorator::Rect clip) const
{
	Clip_guard clip_guard(canvas, clip);

	Rect rect = outer_geometry();
	Area corner(_corner_size, _corner_size);

	Point p1 = rect.p1();
	Point p2 = rect.p2();

	bool const draw_content = false;

	if (draw_content)
		canvas.draw_box(geometry(), Color(10, 20, 40));

	_draw_corner(canvas, Rect(p1, corner), _border_size, true, true,
	             element(Element::TOP_LEFT).color());

	_draw_corner(canvas, Rect(Point(p1.x(), p2.y() - _corner_size + 1), corner),
	             _border_size, true, false,
	             element(Element::BOTTOM_LEFT).color());

	_draw_corner(canvas, Rect(Point(p2.x() - _corner_size + 1, p1.y()), corner),
	             _border_size, false, true,
	             element(Element::TOP_RIGHT).color());

	_draw_corner(canvas, Rect(Point(p2.x() - _corner_size + 1, p2.y() - _corner_size + 1), corner),
	             _border_size, false, false,
	             element(Element::BOTTOM_RIGHT).color());

	_draw_raised_box(canvas, Rect(Point(p1.x() + _corner_size, p1.y()),
	                              Area(rect.w() - 2*_corner_size, _border_size)),
	                              element(Element::TOP).color());

	_draw_raised_box(canvas, Rect(Point(p1.x() + _corner_size, p2.y() - _border_size + 1),
	                              Area(rect.w() - 2*_corner_size, _border_size)),
	                              element(Element::BOTTOM).color());

	_draw_raised_box(canvas, Rect(Point(p1.x(), p1.y() + _corner_size),
	                              Area(_border_size, rect.h() - 2*_corner_size)),
	                              element(Element::LEFT).color());

	_draw_raised_box(canvas, Rect(Point(p2.x() - _border_size + 1, p1.y() + _corner_size),
	                              Area(_border_size, rect.h() - 2*_corner_size)),
	                              element(Element::RIGHT).color());

	Rect title_rect(Point(p1.x() + _border_size, p1.y() + _border_size),
	                Area(rect.w() - 2*_border_size, _title_height));

	_draw_title_box(canvas, title_rect, element(Element::TITLE).color());

	char const * const text = _title.string();;

	Area const label_area(default_font().str_w(text),
	                      default_font().str_h(text));

	Point text_pos = title_rect.center(label_area) - Point(0, 1);

	{
		Clip_guard clip_guard(canvas, title_rect);

		canvas.draw_text(text_pos + Point(1, 1), default_font(),
		                 Color(0, 0, 0, 128), text);

		Color title_color = element(Element::TITLE).color();

		canvas.draw_text(text_pos, default_font(),
		                 Color(255, 255, 255, (2*255 + title_color.r) / 3), text);
	}
}


bool Decorator::Window::update(Genode::Xml_node window_node)
{
	bool updated = Window_base::update(window_node);

	_focused = window_node.has_attribute("focused")
	        && window_node.attribute("focused").has_value("yes");

	try {
		Xml_node highlight = window_node.sub_node("highlight");

		for (unsigned i = 0; i < num_elements(); i++)
			updated |= _apply_state(_elements[i].type(), _focused,
			                        highlight.has_sub_node(_elements[i].type_name()));
	} catch (...) {

		/* window node has no "highlight" sub node, reset highlighting */
		for (unsigned i = 0; i < num_elements(); i++)
			updated |= _apply_state(_elements[i].type(), _focused, false);
	}

	Title title = Decorator::string_attribute(window_node, "title", Title("<untitled>"));
	updated |= !(title == _title);
	_title = title;

	return updated;
}


Decorator::Window_base::Hover Decorator::Window::hover(Point abs_pos) const
{
	Hover hover;

	if (!outer_geometry().contains(abs_pos))
		return hover;

	hover.window_id = id();

	unsigned const x = abs_pos.x() - outer_geometry().x1(),
	               y = abs_pos.y() - outer_geometry().y1();

	Area const area = outer_geometry().area();

	bool const at_border = x <  _border_size
	                    || x >= area.w() - _border_size
	                    || y <  _border_size
	                    || y >= area.h() - _border_size;

	if (at_border) {

		hover.left_sizer   = (x < _corner_size);
		hover.top_sizer    = (y < _corner_size);
		hover.right_sizer  = (x >= area.w() - _corner_size);
		hover.bottom_sizer = (y >= area.h() - _corner_size);

	} else {

		hover.title = (y < _border_size + _title_height);
	}

	return hover;
}


void Decorator::Window::Element::animate()
{
	_r.animate();
	_g.animate();
	_b.animate();

	/* keep animation running until the destination values are reached */
	Animator::Item::animated(_r != _r.dst() || _g != _g.dst() || _b != _b.dst());
}

#endif /* _WINDOW_H_ */
