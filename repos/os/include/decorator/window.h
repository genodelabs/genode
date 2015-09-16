/*
 * \brief  Window representation for decorator
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DECORATOR__WINDOW_H_
#define _INCLUDE__DECORATOR__WINDOW_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>
#include <util/xml_generator.h>
#include <nitpicker_session/client.h>
#include <base/snprintf.h>

/* decorator includes */
#include <decorator/types.h>
#include <decorator/xml_utils.h>


namespace Decorator {
	class Canvas_base;
	class Window_base;
	typedef Genode::List<Window_base> Window_list;
}


class Decorator::Window_base : public Window_list::Element
{
	public:

		struct Border
		{
			unsigned top, left, right, bottom;

			Border(unsigned top, unsigned left, unsigned right, unsigned bottom)
			: top(top), left(left), right(right), bottom(bottom) { }
		};

		struct Hover
		{
			bool left_sizer   = false,
				 right_sizer  = false,
				 top_sizer    = false,
				 bottom_sizer = false,
				 title        = false,
				 closer       = false,
				 minimizer    = false,
				 maximizer    = false,
				 unmaximizer  = false;

			unsigned window_id = 0;

			bool operator != (Hover const &other) const
			{
				return other.left_sizer   != left_sizer
				    || other.right_sizer  != right_sizer
				    || other.top_sizer    != top_sizer
				    || other.bottom_sizer != bottom_sizer
				    || other.title        != title
				    || other.closer       != closer
				    || other.minimizer    != minimizer
				    || other.maximizer    != maximizer
				    || other.unmaximizer  != unmaximizer
				    || other.window_id    != window_id;
			}
		};

		/**
		 * Functor for drawing the elements behind a window
		 *
		 * This functor is used for drawing the decorations of partially
		 * transparent windows. It is implemented by the window stack.
		 */
		struct Draw_behind_fn
		{
			virtual void draw_behind(Canvas_base &, Window_base const &, Rect) const = 0;
		};


	private:

		Nitpicker::Session_client &_nitpicker;

		/*
		 * Geometry of content
		 */
		Rect _geometry;

		/*
		 * Unique window ID
		 */
		unsigned const _id;

		/*
		 * Flag indicating that the current window position has been propagated
		 * to the window's corresponding nitpicker views.
		 */
		bool _nitpicker_views_up_to_date = false;

		/*
		 * Flag indicating that the stacking position of the window within the
		 * window stack has changed. The new stacking position must be
		 * propagated to nitpicker.
		 */
		bool _nitpicker_stacking_up_to_date = false;

		unsigned _topped_cnt = 0;

		bool _global_to_front = false;

		Nitpicker::Session::View_handle _neighbor;

		Border const _border;

		struct Nitpicker_view
		{
			Nitpicker::Session_client      &_nitpicker;
			Nitpicker::Session::View_handle _handle { _nitpicker.create_view() };

			typedef Nitpicker::Session::Command Command;

			Nitpicker_view(Nitpicker::Session_client &nitpicker, unsigned id = 0)
			:
				_nitpicker(nitpicker)
			{
				/*
				 * We supply the window ID as label for the anchor view.
				 */
				if (id) {
					char buf[128];
					Genode::snprintf(buf, sizeof(buf), "%d", id);

					_nitpicker.enqueue<Command::Title>(_handle, buf);
				}
			}

			~Nitpicker_view()
			{
				_nitpicker.destroy_view(_handle);
			}

			Nitpicker::Session::View_handle handle() const { return _handle; }

			void stack(Nitpicker::Session::View_handle neighbor)
			{
				_nitpicker.enqueue<Command::To_front>(_handle, neighbor);
			}

			void place(Rect rect)
			{
				_nitpicker.enqueue<Command::Geometry>(_handle, rect);
				Point offset = Point(0, 0) - rect.p1();
				_nitpicker.enqueue<Command::Offset>(_handle, offset);
			}
		};

		Nitpicker_view _bottom_view, _right_view, _left_view, _top_view;
		Nitpicker_view _content_view;

	public:

		Window_base(unsigned id, Nitpicker::Session_client &nitpicker,
		            Border border)
		:
			_nitpicker(nitpicker), _id(id), _border(border),
			_bottom_view(nitpicker),
			_right_view(nitpicker),
			_left_view(nitpicker),
			_top_view(nitpicker),
			_content_view(nitpicker, _id)
		{ }

		void stack(Nitpicker::Session::View_handle neighbor)
		{
			_neighbor = neighbor;
			_nitpicker_stacking_up_to_date = false;
		}

		Nitpicker::Session::View_handle frontmost_view() const
		{
			return _bottom_view.handle();
		}

		Rect outer_geometry() const
		{
			return Rect(_geometry.p1() - Point(_border.left,  _border.top),
			            _geometry.p2() + Point(_border.right, _border.bottom));
		}

		void border_rects(Rect *top, Rect *left, Rect *right, Rect *bottom) const
		{
			outer_geometry().cut(_geometry, top, left, right, bottom);
		}

		unsigned long id()       const { return _id; }
		Rect          geometry() const { return _geometry; }

		bool is_in_front_of(Window_base const &neighbor) const
		{
			return _neighbor == neighbor.frontmost_view();
		}

		/**
		 * Draw window elements
		 *
		 * \param canvas  graphics back end
		 * \param clip    clipping area to apply
		 */
		virtual void draw(Canvas_base &canvas, Rect clip, Draw_behind_fn const &) const = 0;

		/**
		 * Update internal window representation from XML model
		 *
		 * \return true if window changed
		 *
		 * We do not immediately update the views as part of the update
		 * method because at the time when updating the model, the
		 * decorations haven't been redrawn already. If we updated the
		 * nitpicker views at this point, we would reveal not-yet-drawn pixels.
		 */
		virtual bool update(Xml_node window_node)
		{
			bool result = false;

			/*
			 * Detect the need to bring the window to the top of the global
			 * view stack.
			 */
			unsigned const topped_cnt = attribute(window_node, "topped", 0UL);
			if (topped_cnt != _topped_cnt) {

				_global_to_front               = true;
				_topped_cnt                    = topped_cnt;
				_nitpicker_stacking_up_to_date = false;

				result = true;
			}

			/*
			 * Detect geometry changes
			 */
			Rect new_geometry = rect_attribute(window_node);
			if (new_geometry.p1() != _geometry.p1()
			 || new_geometry.p2() != _geometry.p2()) {

				_geometry = new_geometry;

				_nitpicker_views_up_to_date = false;

				result = true;
			}
			return result;
		}

		virtual void update_nitpicker_views()
		{
			if (!_nitpicker_views_up_to_date) {

				/* update view positions */
				Rect top, left, right, bottom;
				border_rects(&top, &left, &right, &bottom);

				_content_view.place(_geometry);
				_top_view    .place(top);
				_left_view   .place(left);
				_right_view  .place(right);
				_bottom_view .place(bottom);

				_nitpicker_views_up_to_date = true;
			}

			if (!_nitpicker_stacking_up_to_date) {

				/*
				 * Bring the view to the global top of the view stack if the
				 * 'topped' counter changed. Otherwise, we refer to a
				 * session-local neighbor for the restacking operation.
				 */
				Nitpicker::Session::View_handle neighbor = _neighbor;
				if (_global_to_front) {
					neighbor = Nitpicker::Session::View_handle();
					_global_to_front = false;
				}

				_content_view.stack(neighbor);
				_top_view.stack(_content_view.handle());
				_left_view.stack(_top_view.handle());
				_right_view.stack(_left_view.handle());
				_bottom_view.stack(_right_view.handle());

				_nitpicker_stacking_up_to_date = true;
			}
		}

		/**
		 * Report information about element at specified position
		 *
		 * \param position  screen position
		 */
		virtual Hover hover(Point position) const = 0;

		/**
		 * Return true if window needs to be redrawn event if the window layout
		 * model has not changed
		 */
		virtual bool animated() const { return false; }
};

#endif /* _INCLUDE__DECORATOR__WINDOW_H_ */
