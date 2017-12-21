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
#include <ram_session/ram_session.h>
#include <decorator/window.h>
#include <nitpicker_session/connection.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>

/* demo includes */
#include <util/lazy_value.h>

/* gems includes */
#include <gems/animator.h>
#include <gems/nitpicker_buffer.h>

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

		Nitpicker::Session::View_handle _neighbor;

		unsigned _topped_cnt = 0;

		Window_title _title;

		bool _focused = false;

		Lazy_value<int> _alpha = 0;

		Animator &_animator;

		struct Element : Animator::Item
		{
			Theme::Element_type const type;

			char const * const attr;

			bool _highlighted = false;
			bool _present = false;

			Lazy_value<int> alpha = 0;

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
			typedef Nitpicker::Session::View_handle View_handle;

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
		Lazy_value<int> _r, _g, _b;

		Color _color() const { return Color(_r >> 4, _g >> 4, _b >> 4); }

		/**
		 * Nitpicker session that contains the upper and lower window
		 * decorations.
		 */
		Nitpicker::Connection _nitpicker_top_bottom { _env };
		Genode::Constructible<Nitpicker_buffer> _buffer_top_bottom;

		/**
		 * Nitpicker session that contains the left and right window
		 * decorations.
		 */
		Nitpicker::Connection _nitpicker_left_right { _env };
		Genode::Constructible<Nitpicker_buffer> _buffer_left_right;

		Nitpicker_view _bottom_view { _nitpicker, _nitpicker_top_bottom },
		               _right_view  { _nitpicker, _nitpicker_left_right },
		               _left_view   { _nitpicker, _nitpicker_left_right },
		               _top_view    { _nitpicker, _nitpicker_top_bottom };

		Nitpicker_view _content_view { _nitpicker, (unsigned)id() };

		void _reallocate_nitpicker_buffers()
		{
			Area const theme_size = _theme.background_size();
			bool const use_alpha = true;

			Area const inner_size = geometry().area();
			Area const outer_size = outer_geometry().area();

			Area const size_top_bottom(outer_size.w(),
			                           theme_size.h());

			_nitpicker_top_bottom.buffer(Framebuffer::Mode(size_top_bottom.w(),
			                                               size_top_bottom.h(),
			                                               Framebuffer::Mode::RGB565),
			                             use_alpha);

			_buffer_top_bottom.construct(_nitpicker_top_bottom, size_top_bottom,
			                             _env.ram(), _env.rm());

			Area const size_left_right(outer_size.w() - inner_size.w(),
			                           outer_size.h());

			_nitpicker_left_right.buffer(Framebuffer::Mode(size_left_right.w(),
			                                               size_left_right.h(),
			                                               Framebuffer::Mode::RGB565),
			                             use_alpha);

			_buffer_left_right.construct(_nitpicker_left_right, size_left_right,
			                             _env.ram(), _env.rm());
		}

		void _repaint_decorations(Nitpicker_buffer &buffer)
		{
			buffer.reset_surface();

			buffer.apply_to_surface([&] (Pixel_surface &pixel,
			                             Alpha_surface &alpha) {

				_theme.draw_background(pixel, alpha, (int)_alpha);

				_theme.draw_title(pixel, alpha, _title.string());
				
				_for_each_element([&] (Element const &element) {
					_theme.draw_element(pixel, alpha, element.type, element.alpha); });

				Color const tint_color = _color();
				if (tint_color != Color(0, 0, 0))
					Tint_painter::paint(pixel, Rect(Point(0, 0), pixel.size()),
					                    tint_color);
			});

			buffer.flush_surface();

			buffer.nitpicker.framebuffer()->refresh(0, 0, buffer.size().w(), buffer.size().h());
		}

		void _assign_color(Color color)
		{
			_base_color = color;

			_r.dst(_base_color.r << 4, 20);
			_g.dst(_base_color.g << 4, 20);
			_b.dst(_base_color.b << 4, 20);
		}

	public:

		Window(Genode::Env &env, unsigned id, Nitpicker::Session_client &nitpicker,
		       Animator &animator, Theme const &theme, Config const &config)
		:
			Window_base(id),
			Animator::Item(animator),
			_env(env),_theme(theme), _animator(animator),
			_nitpicker(nitpicker), _config(config)
		{
			_reallocate_nitpicker_buffers();
			_alpha.dst(_focused ? 256 : 200, 20);
			animate();
		}

		void stack(Nitpicker::Session::View_handle neighbor) override
		{
			_neighbor = neighbor;

			_top_view.stack(neighbor);
			_left_view.stack(_top_view.handle());
			_right_view.stack(_left_view.handle());
			_bottom_view.stack(_right_view.handle());
			_content_view.stack(_bottom_view.handle());
		}

		Nitpicker::Session::View_handle frontmost_view() const override
		{
			return _content_view.handle();
		}

		Rect _decor_geometry() const
		{
			Theme::Margins const decor = _theme.decor_margins();

			return Rect(geometry().p1() - Point(decor.left, decor.top),
			            geometry().p2() + Point(decor.right, decor.bottom));
		}

		Rect outer_geometry() const override
		{
			Theme::Margins const aura  = _theme.aura_margins();
			Theme::Margins const decor = _theme.decor_margins();

			unsigned const left   = aura.left   + decor.left;
			unsigned const right  = aura.right  + decor.right;
			unsigned const top    = aura.top    + decor.top;
			unsigned const bottom = aura.bottom + decor.bottom;

			return Rect(geometry().p1() - Point(left, top),
			            geometry().p2() + Point(right, bottom));
		}

		void border_rects(Rect *top, Rect *left, Rect *right, Rect *bottom) const
		{
			outer_geometry().cut(geometry(), top, left, right, bottom);
		}

		bool in_front_of(Window_base const &neighbor) const override
		{
			return _neighbor == neighbor.frontmost_view();
		}

		void update_nitpicker_views() override
		{
			if (!_nitpicker_views_up_to_date) {

				Area const theme_size = _theme.background_size();

				/* update view positions */
				Rect top, left, right, bottom;
				border_rects(&top, &left, &right, &bottom);

				_content_view.place(geometry(), Point(0, 0));
				_top_view    .place(top, Point(0, 0));
				_left_view   .place(left, Point(0, -top.h()));
				_right_view  .place(right, Point(-right.w(), -top.h()));
				_bottom_view .place(bottom, Point(0, -theme_size.h() + bottom.h()));

				_nitpicker_views_up_to_date = true;
			}
			_nitpicker.execute();
		}

		void draw(Canvas_base &, Rect, Draw_behind_fn const &) const override { }

		void adapt_to_changed_config()
		{
			_assign_color(_config.base_color(_title));
			animate();
		}

		bool update(Xml_node window_node) override
		{
			bool updated = false;

			/*
			 * Detect the need to bring the window to the top of the global
			 * view stack.
			 */
			unsigned const topped_cnt = attribute(window_node, "topped", 0UL);
			if (topped_cnt != _topped_cnt) {

				_topped_cnt = topped_cnt;

				stack(Nitpicker::Session::View_handle());

				updated = true;
			}

			bool trigger_animation = false;

			Rect const old_geometry = geometry();

			geometry(rect_attribute(window_node));

			/*
			 * Detect position changes
			 */
			if (geometry().p1() != old_geometry.p1()
			 || geometry().p2() != old_geometry.p2()) {

				_nitpicker_views_up_to_date = false;
				updated = true;
			}

			/*
			 * Detect size changes
			 */
			if (geometry().w() != old_geometry.w()
			 || geometry().h() != old_geometry.h()) {

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

			Window_title title = Decorator::string_attribute(window_node, "title",
			                                                 Window_title("<untitled>"));

			if (_title != title) {
				_title = title;
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
			    || _closer.animated() || _maximizer.animated();

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

			_for_each_element([&] (Element &element) { element.animate(); });

			Animator::Item::animated(animated());

			_repaint_decorations(*_buffer_top_bottom);
			_repaint_decorations(*_buffer_left_right);
		}
};

#endif /* _WINDOW_H_ */
