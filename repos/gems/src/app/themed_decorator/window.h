/*
 * \brief  Window decorator that can be styled
 * \author Norman Feske
 * \date   2015-11-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WINDOW_H_
#define _WINDOW_H_

/* Genode includes */
#include <base/ram_allocator.h>
#include <decorator/window.h>
#include <nitpicker_session/connection.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>

/* gems includes */
#include <gems/nitpicker_buffer.h>
#include <gems/animated_geometry.h>

/* local includes */
#include "theme.h"
#include "config.h"
#include "tint_painter.h"

namespace Decorator {

	class Window;
	typedef Genode::String<200> Window_title;
	typedef Genode::Attached_dataspace Attached_dataspace;
}


class Decorator::Window : public Window_base, public Animator::Item
{
	private:

		Genode::Env &_env;

		Theme const &_theme;

		/*
		 * Flag indicating that the current window position has been propagated
		 * to the window's corresponding nitpicker views.
		 */
		bool _nitpicker_views_up_to_date = false;

		unsigned _topped_cnt = 0;

		Window_title _title { };

		bool _focused = false;

		Lazy_value<int> _alpha = 0;

		Animator &_animator;

		class Element : public Animator::Item
		{
			private:

				/*
				 * Noncopyable
				 */
				Element(Element const &);
				Element & operator = (Element const &);

				bool _highlighted = false;
				bool _present = false;

				int _alpha_dst() const
				{
					if (!_present)
						return 0;

					return _highlighted ? 255 : 150;
				}

				void _update_alpha_dst()
				{
					if ((int)alpha == _alpha_dst())
						return;

					alpha.dst(_alpha_dst(), 20);
					animate();
				}

			public:

				Theme::Element_type const type;

				char const * const attr;

				Lazy_value<int> alpha = 0;

				void highlighted(bool highlighted)
				{
					_highlighted = highlighted;
					_update_alpha_dst();
				}

				bool highlighted() const { return _highlighted; }

				void present(bool present)
				{
					_present = present;
					_update_alpha_dst();
				}

				bool present() const { return _present; }

				void animate() override
				{
					alpha.animate();
					animated((int)alpha != alpha.dst());
				}

				Element(Animator &animator, Theme::Element_type type, char const *attr)
				:
					Animator::Item(animator),
					type(type), attr(attr)
				{
					_update_alpha_dst();
				}
		};

		Element _closer    { _animator, Theme::ELEMENT_TYPE_CLOSER,    "closer" };
		Element _maximizer { _animator, Theme::ELEMENT_TYPE_MAXIMIZER, "maximizer" };

		template <typename FN>
		void _for_each_element(FN const &func)
		{
			func(_closer);
			func(_maximizer);
		}

		struct Nitpicker_view
		{
			typedef Nitpicker::Session::Command Command;

			bool const _view_is_remote;

			Nitpicker::Session_client &_nitpicker;

			View_handle _handle;

			Nitpicker_view(Nitpicker::Session_client &nitpicker,
			               unsigned id = 0)
			:
				_view_is_remote(false),
				_nitpicker(nitpicker),
				_handle(_nitpicker.create_view())
			{
				/*
				 * We supply the window ID as label for the anchor view.
				 */
				if (id) {
					char buf[128];
					Genode::snprintf(buf, sizeof(buf), "%d", id);

					_nitpicker.enqueue<Command::Title>(_handle, Genode::Cstring(buf));
				}
			}

			View_handle _create_remote_view(Nitpicker::Session_client &remote_nitpicker)
			{
				/* create view at the remote nitpicker session */
				View_handle handle = remote_nitpicker.create_view();
				Nitpicker::View_capability view_cap = remote_nitpicker.view_capability(handle);

				/* import remote view into local nitpicker session */
				return _nitpicker.view_handle(view_cap);
			}

			/**
			 * Constructor called for creating a view that refers to a buffer
			 * of another nitpicker session
			 */
			Nitpicker_view(Nitpicker::Session_client &nitpicker,
			               Nitpicker::Session_client &remote_nitpicker)
			:
				_view_is_remote(true),
				_nitpicker(nitpicker),
				_handle(_create_remote_view(remote_nitpicker))
			{ }

			~Nitpicker_view()
			{
				if (_view_is_remote)
					_nitpicker.release_view_handle(_handle);
				else
					_nitpicker.destroy_view(_handle);
			}

			View_handle handle() const { return _handle; }

			void stack(View_handle neighbor)
			{
				_nitpicker.enqueue<Command::To_front>(_handle, neighbor);
			}

			void stack_back_most()
			{
				_nitpicker.enqueue<Command::To_back>(_handle, View_handle());
			}

			void place(Rect rect, Point offset)
			{
				_nitpicker.enqueue<Command::Geometry>(_handle, rect);
				_nitpicker.enqueue<Command::Offset>(_handle, offset);
			}
		};

		/**
		 * Nitpicker used as a global namespace of view handles
		 */
		Nitpicker::Session_client &_nitpicker;

		Config const &_config;

		Color _base_color = _config.base_color(_title);

		/*
		 * Color value in 8.4 fixpoint format. We use four bits to
		 * represent the fractional part to enable smooth
		 * interpolation between the color values.
		 */
		Lazy_value<int> _r { }, _g { }, _b { };

		Color _color() const { return Color(_r >> 4, _g >> 4, _b >> 4); }

		bool _show_decoration = _config.show_decoration(_title);

		unsigned _motion = _config.motion(_title);

		Genode::Animated_rect _animated_rect { _animator };

		/*
		 * Geometry most recently propagated to nitpicker
		 */
		Rect _nitpicker_view_rect { };

		/**
		 * Nitpicker session that contains the upper and lower window
		 * decorations.
		 */
		Nitpicker::Connection                _nitpicker_top_bottom { _env };
		Genode::Constructible<Nitpicker_buffer> _buffer_top_bottom { };
		Area                                      _size_top_bottom { };

		/**
		 * Nitpicker session that contains the left and right window
		 * decorations.
		 */
		Nitpicker::Connection                _nitpicker_left_right { _env };
		Genode::Constructible<Nitpicker_buffer> _buffer_left_right { };
		Area                                      _size_left_right { };

		Area _visible_top_bottom_area(Area const inner_size) const
		{
			Area const outer_size = _outer_from_inner_size(inner_size);

			return Area(outer_size.w(), _theme.background_size().h());
		}

		Area _visible_left_right_area(Area const inner_size) const
		{
			Area const outer_size = _outer_from_inner_size(inner_size);

			return Area(outer_size.w() - inner_size.w(), outer_size.h());
		}

		Nitpicker_view _bottom_view { _nitpicker, _nitpicker_top_bottom },
		               _right_view  { _nitpicker, _nitpicker_left_right },
		               _left_view   { _nitpicker, _nitpicker_left_right },
		               _top_view    { _nitpicker, _nitpicker_top_bottom };

		Nitpicker_view _content_view { _nitpicker, (unsigned)id() };

		void _repaint_decorations(Nitpicker_buffer &buffer, Area area)
		{
			buffer.reset_surface();

			buffer.apply_to_surface([&] (Pixel_surface &pixel,
			                             Alpha_surface &alpha) {

				_theme.draw_background(pixel, alpha, area, (int)_alpha);

				_theme.draw_title(pixel, alpha, area, _title.string());
				
				_for_each_element([&] (Element const &element) {
					_theme.draw_element(pixel, alpha, area, element.type, element.alpha); });

				Color const tint_color = _color();
				if (tint_color != Color(0, 0, 0))
					Tint_painter::paint(pixel, Rect(Point(0, 0), area),
					                    tint_color);
			});

			buffer.flush_surface();

			buffer.nitpicker.framebuffer()->refresh(0, 0, buffer.size().w(), buffer.size().h());
		}

		void _repaint_decorations()
		{
			Area const inner_size = _curr_inner_geometry().area();

			_repaint_decorations(*_buffer_top_bottom, _visible_top_bottom_area(inner_size));
			_repaint_decorations(*_buffer_left_right, _visible_left_right_area(inner_size));
		}

		void _reallocate_nitpicker_buffers()
		{
			bool const use_alpha = true;

			Area const size_top_bottom = _visible_top_bottom_area(geometry().area());

			if (size_top_bottom.w() > _size_top_bottom.w()
			 || size_top_bottom.h() > _size_top_bottom.h()
			 || !_buffer_top_bottom.constructed()) {

				_nitpicker_top_bottom.buffer(Framebuffer::Mode(size_top_bottom.w(),
				                                               size_top_bottom.h(),
				                                               Framebuffer::Mode::RGB565),
				                             use_alpha);

				_buffer_top_bottom.construct(_nitpicker_top_bottom, size_top_bottom,
				                             _env.ram(), _env.rm());

				_size_top_bottom = size_top_bottom;
			}

			Area const size_left_right = _visible_left_right_area(geometry().area());

			if (size_left_right.w() > _size_left_right.w()
			 || size_left_right.h() > _size_left_right.h()
			 || !_buffer_left_right.constructed()) {

				_nitpicker_left_right.buffer(Framebuffer::Mode(size_left_right.w(),
				                                               size_left_right.h(),
				                                               Framebuffer::Mode::RGB565),
				                             use_alpha);

				_buffer_left_right.construct(_nitpicker_left_right, size_left_right,
				                             _env.ram(), _env.rm());

				_size_left_right = size_left_right;
			}
		}

		void _assign_color(Color color)
		{
			_base_color = color;

			_r.dst(_base_color.r << 4, 20);
			_g.dst(_base_color.g << 4, 20);
			_b.dst(_base_color.b << 4, 20);
		}

		void _stack_decoration_views()
		{
			if (_show_decoration) {
				_top_view.stack(_content_view.handle());
				_left_view.stack(_top_view.handle());
				_right_view.stack(_left_view.handle());
				_bottom_view.stack(_right_view.handle());
			}
		}

	public:

		Window(Genode::Env &env, unsigned id, Nitpicker::Session_client &nitpicker,
		       Animator &animator, Theme const &theme, Config const &config)
		:
			Window_base(id),
			Animator::Item(animator),
			_env(env), _theme(theme), _animator(animator),
			_nitpicker(nitpicker), _config(config)
		{
			_reallocate_nitpicker_buffers();
			_alpha.dst(_focused ? 256 : 200, 20);
			animate();
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
			return _show_decoration ? _bottom_view.handle() : _content_view.handle();
		}

		/**
		 * Return current inner geometry
		 *
		 * While the window is in motion, the returned rectangle corresponds to
		 * the intermediate window position and size whereas the 'geometry()'
		 * method returns the final geometry.
		 */
		Rect _curr_inner_geometry() const
		{
			return (_motion && _animated_rect.initialized())
			       ? _animated_rect.rect() : geometry();
		}

		Rect _decor_geometry() const
		{
			Theme::Margins const decor = _theme.decor_margins();

			return Rect(_curr_inner_geometry().p1() - Point(decor.left, decor.top),
			            _curr_inner_geometry().p2() + Point(decor.right, decor.bottom));
		}

		Rect _outer_from_inner_geometry(Rect inner) const
		{
			Theme::Margins const aura  = _theme.aura_margins();
			Theme::Margins const decor = _theme.decor_margins();

			unsigned const left   = aura.left   + decor.left;
			unsigned const right  = aura.right  + decor.right;
			unsigned const top    = aura.top    + decor.top;
			unsigned const bottom = aura.bottom + decor.bottom;

			return Rect(inner.p1() - Point(left, top),
			            inner.p2() + Point(right, bottom));
		}

		Area _outer_from_inner_size(Area inner) const
		{
			return _outer_from_inner_geometry(Rect(Point(0, 0), inner)).area();
		}

		Rect outer_geometry() const override
		{
			return _outer_from_inner_geometry(geometry());
		}

		void update_nitpicker_views() override
		{
			bool const nitpicker_view_rect_up_to_date =
				_nitpicker_view_rect.p1() == geometry().p1() &&
				_nitpicker_view_rect.p2() == geometry().p2();

			if (!_nitpicker_views_up_to_date || !nitpicker_view_rect_up_to_date) {

				Area const theme_size = _theme.background_size();
				Rect const inner      = _curr_inner_geometry();
				Rect const outer      = _outer_from_inner_geometry(inner);

				/* update view positions */
				Rect top, left, right, bottom;
				outer.cut(inner, &top, &left, &right, &bottom);

				_content_view.place(inner,  Point(0, 0));
				_top_view    .place(top,    Point(0, 0));
				_left_view   .place(left,   Point(0, -top.h()));
				_right_view  .place(right,  Point(-right.w(), -top.h()));
				_bottom_view .place(bottom, Point(0, -theme_size.h() + bottom.h()));

				_nitpicker.execute();

				_nitpicker_view_rect        = inner;
				_nitpicker_views_up_to_date = true;
			}
		}

		void draw(Canvas_base &, Rect, Draw_behind_fn const &) const override { }

		void adapt_to_changed_config()
		{
			_assign_color(_config.base_color(_title));
			animate();

			_show_decoration = _config.show_decoration(_title);
			_motion          = _config.motion(_title);
		}

		bool update(Xml_node window_node) override
		{
			bool updated = false;

			bool trigger_animation = false;

			Window_title const title =
				window_node.attribute_value("title", Window_title("<untitled>"));

			if (_title != title) {
				_title = title;
				trigger_animation = true;
			}

			_show_decoration = _config.show_decoration(_title);
			_motion          = _config.motion(_title);

			Rect const old_geometry = geometry();
			Rect const new_geometry = rect_attribute(window_node);

			geometry(new_geometry);

			bool const geometry_changed = (old_geometry.p1() != new_geometry.p1())
			                           || (old_geometry.p2() != new_geometry.p2());

			bool const size_changed = (new_geometry.w() != old_geometry.w()
			                        || new_geometry.h() != old_geometry.h());

			bool const motion_triggered =
				_motion && (geometry_changed || !_animated_rect.initialized());

			if (motion_triggered)
				_animated_rect.move_to(new_geometry,
				                       Genode::Animated_rect::Steps{_motion});

			/*
			 * Detect position changes
			 */
			if (geometry_changed || motion_triggered) {
				_nitpicker_views_up_to_date = false;
				updated = true;
			}

			/*
			 * Detect size changes
			 */
			if (size_changed || motion_triggered) {

				_reallocate_nitpicker_buffers();

				/* triggering the animation has the side effect of repainting */
				trigger_animation = true;
			}

			bool focused = window_node.attribute_value("focused", false);

			if (_focused != focused) {
				_focused = focused;
				_alpha.dst(_focused ? 256 : 200, 20);
				trigger_animation = true;
			}

			/* update color on title change as the title is used as policy selector */
			Color const base_color = _config.base_color(_title);
			if (_base_color != base_color) {
				_assign_color(base_color);
				trigger_animation = true;
			}

			_for_each_element([&] (Element &element) {
				bool const present = window_node.attribute_value(element.attr, false);
				if (present != element.present()) {
					element.present(present);
					trigger_animation = true;
				}
			});

			Xml_node const highlight = window_node.has_sub_node("highlight")
			                         ? window_node.sub_node("highlight")
			                         : Xml_node("<highlight/>");

			_for_each_element([&] (Element &element) {
				bool const highlighted = highlight.has_sub_node(element.attr);
				if (highlighted != element.highlighted()) {
					element.highlighted(highlighted);
					trigger_animation = true;
				}
			});

			if (trigger_animation) {
				updated = true;

				/* schedule animation */
				animate();
			}

			return updated;
		}

		Hover hover(Point) const override;

		/**
		 * Window_base interface
		 */
		bool animated() const override
		{
			return (_alpha.dst() != (int)_alpha)
			    || _r != _r.dst() || _g != _g.dst() || _b != _b.dst()
			    || _closer.animated() || _maximizer.animated()
			    || _animated_rect.animated();
		}

		/**
		 * Animator::Item interface
		 */
		void animate() override
		{
			_alpha.animate();
			_r.animate();
			_g.animate();
			_b.animate();
			_animated_rect.animate();

			_for_each_element([&] (Element &element) { element.animate(); });

			_repaint_decorations();

			Animator::Item::animated(animated());
		}
};

#endif /* _WINDOW_H_ */
