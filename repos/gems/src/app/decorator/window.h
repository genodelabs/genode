/*
 * \brief  Example window decorator that mimics the Motif look
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		Gui::Session_client &_gui;

		/*
		 * Flag indicating that the current window position has been propagated
		 * to the window's corresponding GUI views.
		 */
		bool _gui_views_up_to_date = false;

		struct Gui_view
		{
			Gui::Session_client &_gui;

			View_handle _handle { _gui.create_view() };

			typedef Gui::Session::Command Command;

			Gui_view(Gui::Session_client &gui, unsigned id = 0)
			:
				_gui(gui)
			{
				/*
				 * We supply the window ID as label for the anchor view.
				 */
				if (id) {
					char buf[128];
					Genode::snprintf(buf, sizeof(buf), "%d", id);

					_gui.enqueue<Command::Title>(_handle, Genode::Cstring(buf));
				}
			}

			~Gui_view()
			{
				_gui.destroy_view(_handle);
			}

			View_handle handle() const { return _handle; }

			void stack(View_handle neighbor)
			{
				_gui.enqueue<Command::To_front>(_handle, neighbor);
			}

			void stack_back_most()
			{
				_gui.enqueue<Command::To_back>(_handle, View_handle());
			}

			void place(Rect rect)
			{
				_gui.enqueue<Command::Geometry>(_handle, rect);
				Point offset = Point(0, 0) - rect.p1();
				_gui.enqueue<Command::Offset>(_handle, offset);
			}
		};

		Gui_view _bottom_view { _gui },
		         _right_view  { _gui },
		         _left_view   { _gui },
		         _top_view    { _gui };

		Gui_view _content_view { _gui, (unsigned)id() };

		static Border _init_border() {
			return Border(_border_size + _title_height,
			              _border_size, _border_size, _border_size); }

		Border const _border { _init_border() };

		Window_title _title { };

		bool _focused = false;

		Animator &_animator;

		Config const &_config;

		static unsigned const _corner_size = 16;
		static unsigned const _border_size = 4;
		static unsigned const _title_height = 16;

		Color _bright = { 255, 255, 255, 64 };
		Color _dimmed = { 0, 0, 0, 24 };
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
		                        { Element::MAXIMIZER,    _animator, _base_color },
		                        { Element::MINIMIZER,    _animator, _base_color },
		                        { Element::UNMAXIMIZER,  _animator, _base_color } };

		Element &element(Element::Type type)
		{
			return _elements[type];
		}

		Element const &element(Element::Type type) const
		{
			return _elements[type];
		}

		unsigned num_elements() const { return sizeof(_elements)/sizeof(Element); }

		bool _apply_state(Window::Element::Type type,
		                  Window::Element::State const &state)
		{
			return element(type).apply_state(state);
		}

		typedef Config::Window_control Control;

		class Controls
		{
			public:

				enum { MAX_CONTROLS = 10 };

			private:

				Control _controls[MAX_CONTROLS] { };
	
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

		Controls _controls { };


		/***********************
		 ** Drawing utilities **
		 ***********************/

		struct Attr
		{
			Color color;
			bool  pressed;
		};

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

		void _draw_raised_frame(Canvas_base &canvas, Rect rect, bool pressed) const
		{
			Color const top_left_color = pressed ? _dimmed : _bright;

			_draw_hline(canvas, rect.p1(), rect.w(), true, true, 0, top_left_color);
			_draw_vline(canvas, rect.p1(), rect.h(), true, true, 0, top_left_color);

			_draw_hline(canvas, Point(rect.p1().x(), rect.p2().y()), rect.w(),
			            true, true, 0, _dark);
			_draw_vline(canvas, Point(rect.p2().x(), rect.p1().y()), rect.h(),
			            true, true, 0, _dark);
		}

		void _draw_raised_box(Canvas_base &canvas, Rect rect, Attr attr) const
		{
			canvas.draw_box(rect, attr.color);
			_draw_raised_frame(canvas, rect, attr.pressed);
		}

		static Color _mix_colors(Color c1, Color c2, int alpha)
		{
			return Color((c1.r*alpha + c2.r*(255 - alpha)) >> 8,
			             (c1.g*alpha + c2.g*(255 - alpha)) >> 8,
			             (c1.b*alpha + c2.b*(255 - alpha)) >> 8);
		}

		void _draw_title_box(Canvas_base &canvas, Rect rect, Attr attr) const
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

			Color const upper_color = attr.pressed ? Color(0, 0, 0) : Color(255, 255, 255);
			Color const lower_color = attr.pressed ? Color(127, 127, 127) : Color(0, 0, 0);

			for (unsigned i = 0; i < rect.h(); i++) {

				bool const upper_half = (int)i < mid_y;

				int const alpha = upper_half
				                ? (ascent*(mid_y - i)) >> 8
				                : (ascent*(i - mid_y)) >> 8;

				Color const mix_color  = upper_half ? upper_color : lower_color;
				Color const line_color = _mix_colors(mix_color, attr.color, alpha);

				canvas.draw_box(Rect(rect.p1() + Point(0, i),
				                     Area(rect.w(), 1)), line_color);
			}

			_draw_raised_frame(canvas, rect, attr.pressed);
		}

		void _draw_corner(Canvas_base &canvas, Rect const rect,
		                  unsigned const border,
		                  bool const left, bool const top, Attr const attr) const
		{
			bool const bottom = !top;
			bool const right  = !left;

			int const x1 = rect.p1().x();
			int const y1 = rect.p1().y();
			int const x2 = rect.p2().x();
			int const y2 = rect.p2().y();
			int const w  = rect.w();
			int const h  = rect.h();

			Color const top_left_color = attr.pressed ? _dimmed : _bright;

			canvas.draw_box(Rect(Point(x1, top ? y1 : y2 - border + 1),
			                     Area(w, border)), attr.color);

			canvas.draw_box(Rect(Point(left ? x1 : x2 - border + 1,
			                           top  ? y1 + border : y1),
			                     Area(border, h - border)), attr.color);

			/* top bright line */
			_draw_hline(canvas, rect.p1(), w,
			            top || left, top || right, border, top_left_color);

			/* inner horizontal line */
			int y = top ? y1 + border - 1 : y2 - border + 1;
			_draw_hline(canvas, Point(x1, y), w, right, left, w - border,
			            top ? _dark : top_left_color);

			/* bottom line */
			_draw_hline(canvas, Point(x1, y2), w,
			            bottom || left, bottom || right, border, _dark);

			/* left bright line */
			_draw_vline(canvas, rect.p1(), h,
			            left || top, left || bottom, border, top_left_color);

			/* inner vertical line */
			int x = left ? x1 + border - 1 : x2 - border + 1;
			_draw_vline(canvas, Point(x, y1), h, bottom, top, h - border + 1,
			            left ? _dark : top_left_color);

			/* right line */
			_draw_vline(canvas, Point(x2, y1), h,
			            right || top, right || bottom, border, _dark);
		}

		Attr _window_elem_attr(Element::Type type) const
		{
			return Attr { .color   = element(type).color(),
			              .pressed = element(type).pressed() };
		}

		Attr _window_control_attr(Control const &window_control) const
		{
			switch (window_control.type()) {
			case Control::TYPE_CLOSER:      return _window_elem_attr(Element::CLOSER);
			case Control::TYPE_MAXIMIZER:   return _window_elem_attr(Element::MAXIMIZER);
			case Control::TYPE_MINIMIZER:   return _window_elem_attr(Element::MINIMIZER);
			case Control::TYPE_UNMAXIMIZER: return _window_elem_attr(Element::UNMAXIMIZER);
			case Control::TYPE_TITLE:       return _window_elem_attr(Element::TITLE);
			case Control::TYPE_UNDEFINED:   break;
			};
			return Attr { .color = Color(0, 0, 0), .pressed = false };
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
			_draw_title_box(canvas, rect, _window_control_attr(control));

			canvas.draw_texture(rect.p1() + Point(1,1),
			                    _window_control_texture(control));
		}

		void _stack_decoration_views()
		{
			_top_view.stack(_content_view.handle());
			_left_view.stack(_top_view.handle());
			_right_view.stack(_left_view.handle());
			_bottom_view.stack(_right_view.handle());
		}

	public:

		Window(unsigned id, Gui::Session_client &gui,
		       Animator &animator, Config const &config)
		:
			Window_base(id),
			_gui(gui),
			_animator(animator), _config(config)
		{ }

		/**
		 * Return border margins of floating window
		 */
		static Border border_floating()
		{
			return Border(_border_size + _title_height,
			              _border_size, _border_size, _border_size);
		}

		void stack(View_handle neighbor) override
		{
			_content_view.stack(neighbor);
			_stack_decoration_views();
		}

		void stack_front_most() override
		{
			_content_view.stack(View_handle());
			_stack_decoration_views();
		}

		void stack_back_most() override
		{
			_content_view.stack_back_most();
			_stack_decoration_views();
		}

		View_handle frontmost_view() const override
		{
			return _bottom_view.handle();
		}

		Rect outer_geometry() const override
		{
			return Rect(geometry().p1() - Point(_border.left,  _border.top),
			            geometry().p2() + Point(_border.right, _border.bottom));
		}

		void border_rects(Rect *top, Rect *left, Rect *right, Rect *bottom) const
		{
			outer_geometry().cut(geometry(), top, left, right, bottom);
		}

		void update_gui_views() override
		{
			if (!_gui_views_up_to_date) {

				/* update view positions */
				Rect top, left, right, bottom;
				border_rects(&top, &left, &right, &bottom);

				_content_view.place(geometry());
				_top_view    .place(top);
				_left_view   .place(left);
				_right_view  .place(right);
				_bottom_view .place(bottom);

				_gui_views_up_to_date = true;
			}
		}

		void adapt_to_changed_config()
		{
			_base_color = _config.base_color(_title);
		}

		void draw(Canvas_base &canvas, Rect clip, Draw_behind_fn const &) const override;

		bool update(Xml_node) override;

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
