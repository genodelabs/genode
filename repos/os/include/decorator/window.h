/*
 * \brief  Window representation for decorator
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DECORATOR__WINDOW_H_
#define _INCLUDE__DECORATOR__WINDOW_H_

/* Genode includes */
#include <util/string.h>
#include <util/xml_generator.h>
#include <util/list_model.h>
#include <util/reconstructible.h>
#include <gui_session/client.h>

/* decorator includes */
#include <decorator/types.h>
#include <decorator/xml_utils.h>


namespace Decorator {
	class Canvas_base;
	class Window_base;

	using Abandoned_windows = Genode::List<Genode::List_element<Window_base> >;
	using Reversed_windows  = Genode::List<Genode::List_element<Window_base> >;
}


class Decorator::Window_base : private Genode::List_model<Window_base>::Element
{
	public:

		using View_handle = Gui::Session::View_handle;

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
		struct Draw_behind_fn : Interface
		{
			virtual void draw_behind(Canvas_base &, Window_base const &, Rect) const = 0;
		};

	private:

		/* allow 'List_model' to access 'List_model::Element' */
		friend class Genode::List_model<Window_base>;
		friend class Genode::List<Window_base>;

		/*
		 * Geometry of content
		 */
		Rect _geometry { };

		/*
		 * Unique window ID
		 */
		unsigned const _id;

		bool _stacked = false;

		/*
		 * View immediately behind the window
		 */
		Genode::Constructible<View_handle> _neighbor { };

		Genode::List_element<Window_base> _abandoned { this };

		Genode::List_element<Window_base> _reversed { this };

	public:

		Window_base(unsigned id) : _id(id) { }

		virtual ~Window_base() { }

		void abandon(Abandoned_windows &abandoned_windows)
		{
			abandoned_windows.insert(&_abandoned);
		}

		void prepend_to_reverse_list(Reversed_windows &window_list)
		{
			window_list.insert(&_reversed);
		}

		using List_model<Window_base>::Element::next;

		unsigned id()       const { return _id; }
		Rect     geometry() const { return _geometry; }

		void stacking_neighbor(View_handle neighbor)
		{
			_neighbor.construct(neighbor);
			_stacked  = true;
		}

		bool back_most() const
		{
			return _stacked && !_neighbor.constructed();
		}

		bool in_front_of(Window_base const &neighbor) const
		{
			return _stacked
			    && _neighbor.constructed()
			    && (*_neighbor == neighbor.frontmost_view());
		}

		void geometry(Rect geometry) { _geometry = geometry; }

		virtual Rect outer_geometry() const = 0;

		virtual void stack(View_handle neighbor) = 0;

		virtual void stack_front_most() = 0;

		virtual void stack_back_most() = 0;

		virtual View_handle frontmost_view() const = 0;

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
		 * GUI views at this point, we would reveal not-yet-drawn pixels.
		 */
		virtual bool update(Xml_node window_node) = 0;

		virtual void update_gui_views() { }

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

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return _id == node.attribute_value("id", ~0UL);
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &) { return true; }
};

#endif /* _INCLUDE__DECORATOR__WINDOW_H_ */
