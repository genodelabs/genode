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
#include <gui_session/connection.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>

/* gems includes */
#include <gems/gui_buffer.h>
#include <gems/animated_geometry.h>

/* local includes */
#include "theme.h"
#include "config.h"
#include "tint_painter.h"

namespace Decorator {

	class Window;
	using Window_title = Genode::String<200>;
	using Attached_dataspace = Genode::Attached_dataspace;
}


class Decorator::Window : public Window_base, public Animator::Item
{
	private:

		Genode::Env &_env;

		Theme const &_theme;

		/*
		 * Flag indicating that the current window position has been propagated
		 * to the window's corresponding GUI views.
		 */
		bool _gui_views_up_to_date = false;

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

		struct Gui_view
		{
			using Command = Gui::Session::Command;

			bool const _view_is_remote;

			Gui::Connection &_gui;

			Gui::View_id _id;

			Gui_view(Gui::Connection &gui, unsigned win_id = 0)
			:
				_view_is_remote(false),
				_gui(gui),
				_id(_gui.create_view())
			{
				/*
				 * We supply the window ID as label for the anchor view.
				 */
				if (win_id)
					_gui.enqueue<Command::Title>(_id,
					                             Genode::String<128>(win_id).string());
			}

			Gui::View_id _create_remote_view(Gui::Connection &remote_gui)
			{
				/* create view at the remote GUI session */
				Gui::View_id id = remote_gui.create_view();
				Gui::View_capability view_cap = remote_gui.view_capability(id);

				/* import remote view into local GUI session */
				return _gui.alloc_view_id(view_cap);
			}

			/**
			 * Constructor called for creating a view that refers to a buffer
			 * of another GUI session
			 */
			Gui_view(Gui::Connection &gui,
			         Gui::Connection &remote_gui)
			:
				_view_is_remote(true),
				_gui(gui),
				_id(_create_remote_view(remote_gui))
			{ }

			~Gui_view()
			{
				if (_view_is_remote)
					_gui.release_view_id(_id);
				else
					_gui.destroy_view(_id);
			}

			Gui::View_id id() const { return _id; }

			void stack(Gui::View_id neighbor)
			{
				_gui.enqueue<Command::Front_of>(_id, neighbor);
			}

			void stack_front_most() { _gui.enqueue<Command::Front>(_id); }

			void stack_back_most()  { _gui.enqueue<Command::Back>(_id); }

			void place(Rect rect, Point offset)
			{
				_gui.enqueue<Command::Geometry>(_id, rect);
				_gui.enqueue<Command::Offset>(_id, offset);
			}
		};

		/**
		 * GUI session used as a global namespace of view ID
		 */
		Gui::Connection &_gui;

		Config const &_config;

		Color _base_color = _config.base_color(_title);

		/*
		 * Color value in 8.4 fixpoint format. We use four bits to
		 * represent the fractional part to enable smooth
		 * interpolation between the color values.
		 */
		Lazy_value<int> _r { }, _g { }, _b { };

		Color _color() const { return Color::clamped_rgb(_r >> 4, _g >> 4, _b >> 4); }

		bool _show_decoration = _config.show_decoration(_title);

		unsigned _motion = _config.motion(_title);

		Genode::Animated_rect _animated_rect { _animator };

		/*
		 * Geometry most recently propagated to GUI server
		 */
		Rect _gui_view_rect { };

		/**
		 * GUI session that contains the upper and lower window decorations
		 */
		Gui::Connection                   _gui_top_bottom { _env };
		Genode::Constructible<Gui_buffer> _buffer_top_bottom { };
		Area                              _size_top_bottom { };

		/**
		 * GUI session that contains the left and right window decorations
		 */
		Gui::Connection                   _gui_left_right { _env };
		Genode::Constructible<Gui_buffer> _buffer_left_right { };
		Area                              _size_left_right { };

		Area _visible_top_bottom_area(Area const inner_size) const
		{
			Area const outer_size = _outer_from_inner_size(inner_size);

			return Area(outer_size.w, _theme.background_size().h);
		}

		Area _visible_left_right_area(Area const inner_size) const
		{
			Area const outer_size = _outer_from_inner_size(inner_size);

			return Area(outer_size.w - inner_size.w, outer_size.h);
		}

		Gui_view _bottom_view { _gui, _gui_top_bottom },
		         _right_view  { _gui, _gui_left_right },
		         _left_view   { _gui, _gui_left_right },
		         _top_view    { _gui, _gui_top_bottom };

		Gui_view _content_view { _gui, (unsigned)id() };

		void _repaint_decorations(Gui_buffer &buffer, Area area)
		{
			buffer.reset_surface();

			buffer.apply_to_surface([&] (Pixel_surface &pixel,
			                             Alpha_surface &alpha) {

				_theme.draw_background(pixel, alpha, area, (int)_alpha);

				_theme.draw_title(pixel, alpha, area, _title.string());
				
				_for_each_element([&] (Element const &element) {
					_theme.draw_element(pixel, alpha, area, element.type, element.alpha); });

				Color const tint_color = _color();
				if (tint_color != Color::black())
					Tint_painter::paint(pixel, Rect(Point(0, 0), area),
					                    tint_color);
			});

			buffer.flush_surface();

			buffer.gui.framebuffer.refresh(0, 0, buffer.size().w, buffer.size().h);
		}

		void _repaint_decorations()
		{
			Area const inner_size = _curr_inner_geometry().area;

			_repaint_decorations(*_buffer_top_bottom, _visible_top_bottom_area(inner_size));
			_repaint_decorations(*_buffer_left_right, _visible_left_right_area(inner_size));
		}

		void _reallocate_gui_buffers()
		{
			bool const use_alpha = true;

			Area const size_top_bottom = _visible_top_bottom_area(geometry().area);

			if (size_top_bottom.w > _size_top_bottom.w
			 || size_top_bottom.h > _size_top_bottom.h
			 || !_buffer_top_bottom.constructed()) {

				_gui_top_bottom.buffer(Framebuffer::Mode { .area = { size_top_bottom.w,
				                                                     size_top_bottom.h } },
				                       use_alpha);

				_buffer_top_bottom.construct(_gui_top_bottom, size_top_bottom,
				                             _env.ram(), _env.rm());

				_size_top_bottom = size_top_bottom;
			}

			Area const size_left_right = _visible_left_right_area(geometry().area);

			if (size_left_right.w > _size_left_right.w
			 || size_left_right.h > _size_left_right.h
			 || !_buffer_left_right.constructed()) {

				_gui_left_right.buffer(Framebuffer::Mode { .area = { size_left_right.w,
				                                                     size_left_right.h } },
				                       use_alpha);

				_buffer_left_right.construct(_gui_left_right, size_left_right,
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
				_top_view.stack(_content_view.id());
				_left_view.stack(_top_view.id());
				_right_view.stack(_left_view.id());
				_bottom_view.stack(_right_view.id());
			}
		}

	public:

		Window(Genode::Env &env, unsigned id, Gui::Connection &gui,
		       Animator &animator, Theme const &theme, Config const &config)
		:
			Window_base(id),
			Animator::Item(animator),
			_env(env), _theme(theme), _animator(animator),
			_gui(gui), _config(config)
		{
			_reallocate_gui_buffers();
			_alpha.dst(_focused ? 256 : 200, 20);
			animate();
		}

		void stack(Gui::View_id neighbor) override
		{
			_content_view.stack(neighbor);
			_stack_decoration_views();

		}
		void stack_front_most() override
		{
			_content_view.stack_front_most();
			_stack_decoration_views();
		}

		void stack_back_most() override
		{
			_content_view.stack_back_most();
			_stack_decoration_views();
		}

		Gui::View_id frontmost_view() const override
		{
			return _show_decoration ? _bottom_view.id() : _content_view.id();
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

			return Rect::compound(_curr_inner_geometry().p1() - Point(decor.left,  decor.top),
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

			return Rect::compound(inner.p1() - Point(left, top),
			                      inner.p2() + Point(right, bottom));
		}

		Area _outer_from_inner_size(Area inner) const
		{
			return _outer_from_inner_geometry(Rect(Point(0, 0), inner)).area;
		}

		Rect outer_geometry() const override
		{
			return _outer_from_inner_geometry(geometry());
		}

		void update_gui_views() override
		{
			bool const gui_view_rect_up_to_date =
				_gui_view_rect.p1() == geometry().p1() &&
				_gui_view_rect.p2() == geometry().p2();

			if (!_gui_views_up_to_date || !gui_view_rect_up_to_date) {

				Area const theme_size = _theme.background_size();
				Rect const inner      = _curr_inner_geometry();
				Rect const outer      = _outer_from_inner_geometry(inner);

				/* update view positions */
				Rect::Cut_remainder const r = outer.cut(inner);

				_content_view.place(inner,  Point(0, 0));
				_top_view    .place(r.top,    Point(0, 0));
				_left_view   .place(r.left,   Point(0, -r.top.h()));
				_right_view  .place(r.right,  Point(-r.right.w(), -r.top.h()));
				_bottom_view .place(r.bottom, Point(0, -theme_size.h + r.bottom.h()));

				_gui.execute();

				_gui_view_rect        = inner;
				_gui_views_up_to_date = true;
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
			Rect const new_geometry = Rect::from_xml(window_node);

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
				_gui_views_up_to_date = false;
				updated = true;
			}

			/*
			 * Detect size changes
			 */
			if (size_changed || motion_triggered) {

				_reallocate_gui_buffers();

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
