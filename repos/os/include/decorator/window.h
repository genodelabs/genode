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
#include <base/registry.h>
#include <base/id_space.h>

/* decorator includes */
#include <decorator/types.h>


namespace Decorator {
	class Canvas_base;
	class Window_base;

	using Windows           = Id_space<Window_base>;
	using Abandoned_windows = Registry<Window_base>;
	using Reversed_windows  = Genode::List<Genode::List_element<Window_base> >;
}


class Decorator::Window_base : private Windows::Element
{
	public:

		using Windows::Element::id;

		struct Ref;

		using Refs = List_model<Ref>;

		/**
		 * Reference to a window
		 *
		 * The 'Ref' type decouples the lifetime of window objects from
		 * the lifetimes of their surrounding boundaries. If a window
		 * moves from one boundary to another, the old 'Ref' vanishes and
		 * a new 'Ref' is created but the window object stays intact.
		 */
		struct Ref : Refs::Element
		{
			Window_base &window;

			Registry<Ref>::Element _registered;

			inline  Ref(Window_base &);

			/**
			 * List_model::Element
			 */
			inline bool matches(Node const &) const;

			/**
			 * List_model::Element
			 */
			static bool type_matches(Node const &) { return true; }
		};

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

			Windows::Id window_id { };

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
			virtual void draw_behind(Canvas_base &, Ref const &, Rect) const = 0;
		};

	private:

		/* allow 'List_model' to access 'List_model::Element' */
		friend class Genode::List_model<Window_base>;
		friend class Genode::List<Window_base>;

		Registry<Ref> _refs { };

		/*
		 * Geometry of content
		 */
		Rect _geometry { };

		bool _stacked = false;

		/*
		 * View immediately behind the window
		 */
		Genode::Constructible<Gui::View_id> _neighbor { };

		Constructible<Abandoned_windows::Element> _abandoned { };

		Genode::List_element<Window_base> _reversed { this };

	public:

		Window_base(Windows &windows, Windows::Id id)
		:
			Windows::Element(*this, windows, id)
		{ }

		virtual ~Window_base() { }

		bool referenced() const
		{
			bool result = false;
			_refs.for_each([&] (Ref const &) { result = true; });
			return result;
		}

		void consider_as_abandoned(Abandoned_windows &registry)
		{
			_abandoned.construct(registry, *this);
		}

		/**
		 * Revert 'consider_as_abandoned' after window was temporarily not referenced
		 */
		void dont_abandon() { _abandoned.destruct(); }

		void prepend_to_reverse_list(Reversed_windows &window_list)
		{
			window_list.insert(&_reversed);
		}

		Rect geometry() const { return _geometry; }

		void stacking_neighbor(Gui::View_id neighbor)
		{
			_neighbor.construct(neighbor);
			_stacked  = true;
		}

		void forget_neighbor() { _neighbor.destruct(); }

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

		virtual void stack(Gui::View_id neighbor) = 0;

		virtual void stack_front_most() = 0;

		virtual void stack_back_most() = 0;

		virtual Gui::View_id frontmost_view() const = 0;

		/**
		 * Draw window elements
		 */
		virtual void draw(Canvas_base &, Ref const &, Rect clip, Draw_behind_fn const &) const = 0;

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
		virtual bool update(Node const &window_node) = 0;

		struct Clip : Rect { };

		virtual void update_gui_views(Clip const &) { }

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
		bool matches(Node const &node) const
		{
			return id() == Windows::Id { node.attribute_value("id", ~0UL) };
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Node const &) { return true; }
};


Decorator::Window_base::Ref::Ref(Window_base &window)
:
	window(window), _registered(window._refs, *this)
{ }


bool Decorator::Window_base::Ref::matches(Node const &node) const
{
	return window.id() == Windows::Id { node.attribute_value("id", ~0UL) };
}

#endif /* _INCLUDE__DECORATOR__WINDOW_H_ */
