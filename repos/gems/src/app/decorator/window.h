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

/* Genode includes */
#include <decorator/window.h>

/* local includes */
#include "config.h"
#include "window_element.h"


namespace Decorator { class Window; }


class Decorator::Window : public Window_base
{
	private:

		Window_title _title;

		bool _focused = false;

		Animator &_animator;

		Config const &_config;

		static unsigned const _corner_size = 16;
		static unsigned const _border_size = 4;
		static unsigned const _title_height = 16;

		static Border _border() {
			return Border(_border_size + _title_height,
			              _border_size, _border_size, _border_size); }

		Color _bright = { 255, 255, 255, 64 };
		Color _dark   = { 0, 0, 0, 127 };

		Color _base_color = _config.base_color(_title);

		bool _has_alpha = false;

		Area const _icon_size { 16, 16 };


		/*
		 * Intensity of the title-bar radient in percent. A value of 0 produces
		 * no gradient. A value of 100 creates a gradient from white over
		 * 'color' to black.
		 */
		Lazy_value<int> _gradient_percent = _config.gradient_percent(_title);

		typedef Window_element Element;

		/*
		 * The element order must correspond to the order of enum values
		 * because the type is used as index into the '_elements' array.
		 */
		Element _elements[13] { { Element::TITLE,        _animator, _base_color },
		                        { Element::LEFT,         _animator, _base_color },
		                        { Element::RIGHT,        _animator, _base_color },
		                        { Element::TOP,          _animator, _base_color },
		                        { Element::BOTTOM,       _animator, _base_color },
		                        { Element::TOP_LEFT,     _animator, _base_color },
		                        { Element::TOP_RIGHT,    _animator, _base_color },
		                        { Element::BOTTOM_LEFT,  _animator, _base_color },
		                        { Element::BOTTOM_RIGHT, _animator, _base_color },
		                        { Element::CLOSER,       _animator, _base_color },
		                        { Element::MAXIMIZE,     _animator, _base_color },
		                        { Element::MINIMIZE,     _animator, _base_color },
		                        { Element::UNMAXIMIZE,   _animator, _base_color } };

		Element &element(Element::Type type)
		{
			return _elements[type];
		}

		Element const &element(Element::Type type) const
		{
			return _elements[type];
		}

		unsigned num_elements() const { return sizeof(_elements)/sizeof(Element); }

		bool _apply_state(Window::Element::Type type, bool focused, bool highlighted)
		{
			return element(type).apply_state(_focused, highlighted, _base_color);
		}

		typedef Config::Window_control Control;

		class Controls
		{
			public:

				enum { MAX_CONTROLS = 10 };

			private:

				Control _controls[MAX_CONTROLS];
	
				unsigned _num = 0;

			public:

				/**
				 * Add window control
				 */
				void add(Control control)
				{
					if (_num < MAX_CONTROLS)
						_controls[_num++] = control;
				}

				unsigned num() const { return _num; }

				class Index_out_of_range { };

				/**
				 * Obtain Nth window control
				 */
				Control control(unsigned n) const
				{
					if (n >= MAX_CONTROLS)
						throw Index_out_of_range();

					return _controls[n];
				}

				bool operator != (Controls const &other) const
				{
					if (_num != other._num) return true;

					for (unsigned i = 0; i < _num; i++)
						if (_controls[i] != other._controls[i])
							return true;

					return false;
				}
		};

		Controls _controls;


		/***********************
		 ** Drawing utilities **
		 ***********************/

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

		static Color _mix_colors(Color c1, Color c2, int alpha)
		{
			return Color((c1.r*alpha + c2.r*(255 - alpha)) >> 8,
			             (c1.g*alpha + c2.g*(255 - alpha)) >> 8,
			             (c1.b*alpha + c2.b*(255 - alpha)) >> 8);
		}

		void _draw_title_box(Canvas_base &canvas, Rect rect, Color color) const
		{
			/*
			 * Produce gradient such that the upper half becomes brighter and
			 * the lower half becomes darker. The gradient is created by mixing
			 * the base color with white (for the upper half) and black (for
			 * the lower half).
			 */

			/* alpha ascent as 8.8 fixpoint number */
			int const ascent = (_gradient_percent*255 << 8) / (rect.h()*100);

			int const mid_y = rect.h() / 2;

			Color const white(255, 255, 255), black(0, 0, 0);

			for (unsigned i = 0; i < rect.h(); i++) {

				bool const upper_half = (int)i < mid_y;

				int const alpha = upper_half
				                ? (ascent*(mid_y - i)) >> 8
				                : (ascent*(i - mid_y)) >> 8;

				Color const line_color =
					_mix_colors(upper_half ? white : black, color, alpha);

				canvas.draw_box(Rect(rect.p1() + Point(0, i),
				                     Area(rect.w(), 1)), line_color);
			}

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

		Color _window_control_color(Control window_control) const
		{
			switch (window_control.type()) {
			case Control::TYPE_CLOSER:      return element(Element::CLOSER).color();
			case Control::TYPE_MAXIMIZER:   return element(Element::MAXIMIZE).color();
			case Control::TYPE_MINIMIZER:   return element(Element::MINIMIZE).color();
			case Control::TYPE_UNMAXIMIZER: return element(Element::UNMAXIMIZE).color();
			case Control::TYPE_TITLE:       return element(Element::TITLE).color();
			case Control::TYPE_UNDEFINED:   break;
			};
			return Color(0, 0, 0);
		}

		Texture_id _window_control_texture(Control window_control) const
		{
			switch (window_control.type()) {
			case Control::TYPE_CLOSER:      return TEXTURE_ID_CLOSER;
			case Control::TYPE_MAXIMIZER:   return TEXTURE_ID_MAXIMIZE;
			case Control::TYPE_MINIMIZER:   return TEXTURE_ID_MINIMIZE;
			case Control::TYPE_UNMAXIMIZER: return TEXTURE_ID_WINDOWED;
			case Control::TYPE_TITLE:
			case Control::TYPE_UNDEFINED:
				break;
			};

			class No_texture_for_window_control { };
			throw No_texture_for_window_control();
		}

		void _draw_window_control(Canvas_base &canvas, Rect rect,
		                          Control control) const
		{
			_draw_title_box(canvas, rect, _window_control_color(control));

			canvas.draw_texture(rect.p1() + Point(1,1),
			                    _window_control_texture(control));
		}

	public:

		Window(unsigned id, Nitpicker::Session_client &nitpicker,
		       Animator &animator, Config const &config)
		:
			Window_base(id, nitpicker, _border()),
			_animator(animator), _config(config)
		{ }

		void adapt_to_changed_config()
		{
			_base_color = _config.base_color(_title);
		}

		void draw(Canvas_base &canvas, Rect clip, Draw_behind_fn const &) const override;

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

#endif /* _WINDOW_H_ */
