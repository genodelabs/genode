/*
 * \brief  Window-stack handling for decorator
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DECORATOR__WINDOW_STACK_H_
#define _INCLUDE__DECORATOR__WINDOW_STACK_H_

/* Genode includes */
#include <gui_session/gui_session.h>
#include <base/log.h>
#include <base/allocator.h>

/* local includes */
#include <decorator/types.h>
#include <decorator/window.h>
#include <decorator/window_factory.h>
#include <decorator/xml_utils.h>

namespace Decorator { class Window_stack; }


class Decorator::Window_stack : public Window_base::Draw_behind_fn
{
	private:

		struct Boundary;
		using Boundaries = List_model<Boundary>;

		using Abandoned_boundaries = Registry<Boundary>;

		using Win_ref = Window_base::Ref;

		struct Boundary : Boundaries::Element
		{
			using Name = Genode::String<64>;

			Name const _name;

			Constructible<Registry<Boundary>::Element> _abandoned { };

			Rect rect { };

			List_model<Win_ref> win_refs { };

			Boundary(Name const &name) : _name(name) { }

			void update(Window_factory_base &factory,
			            Abandoned_windows   &abandoned_windows,
			            Dirty_rect          &dirty_rect,
			            Xml_node      const &boundary)
			{
				rect = Rect::from_xml(boundary);

				win_refs.update_from_xml(boundary,

					[&] (Xml_node const &node) -> Win_ref & {
						return factory.create_ref(node); },

					[&] (Win_ref &ref) {
						Window_base &window = ref.window;
						factory.destroy_ref(ref);
						if (!window.referenced())
							window.consider_as_abandoned(abandoned_windows);
					},

					[&] (Win_ref &ref, Xml_node const &node) {
						Rect const orig_geometry = ref.window.outer_geometry();
						if (ref.window.update(node)) {
							dirty_rect.mark_as_dirty(orig_geometry);
							dirty_rect.mark_as_dirty(ref.window.outer_geometry());
						}
					}
				);
			}

			void abandon(Registry<Boundary> &registry)
			{
				_abandoned.construct(registry, *this);
			}

			static Name name(Xml_node const &node)
			{
				return node.attribute_value("name", Name());
			}

			/**
			 * List_model::Element
			 */
			bool matches(Xml_node const &node) const
			{
				return _name == name(node);
			}

			/**
			 * List_model::Element
			 */
			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("boundary");
			}

			/**
			 * Generate window list in reverse order
			 */
			Reversed_windows reversed_window_list()
			{
				Reversed_windows reversed { };
				win_refs.for_each([&] (Win_ref &ref) {
					ref.window.prepend_to_reverse_list(reversed); });
				return reversed;
			}
		};

		Boundaries           _boundaries { };
		Window_factory_base &_window_factory;
		Allocator           &_alloc;
		Dirty_rect mutable   _dirty_rect { };

		Windows::Id _front_most_id { ~0UL };

		inline void _draw_rec(Canvas_base &canvas, Win_ref const *,
		                      Rect rect) const;

		static inline
		void _with_window_xml(Genode::Xml_node const &node, unsigned id,
		                      auto const &fn, auto const &missing_fn)
		{
			bool found = false;
			node.for_each_sub_node("window", [&] (Xml_node const &window) {
				if (!found && node.attribute_value("id", 0UL) == id) {
					found = true;
					fn(window);
				}
			});
			if (!found)
				missing_fn();
		}

		void _for_each_window_const(auto const &fn) const
		{
			_boundaries.for_each([&] (Boundary const &boundary) {
				boundary.win_refs.for_each([&] (Win_ref const &ref) {
					fn(ref.window); }); });
		}

	public:

		Window_stack(Window_factory_base &window_factory, Allocator &alloc)
		:
			_window_factory(window_factory), _alloc(alloc)
		{ }

		void mark_as_dirty(Rect rect) { _dirty_rect.mark_as_dirty(rect); }

		void for_each_window(auto const &fn)
		{
			_boundaries.for_each([&] (Boundary &boundary) {
				boundary.win_refs.for_each([&] (Win_ref &ref) {
					fn(ref.window); }); });
		}

		Dirty_rect draw(Canvas_base &canvas) const
		{
			Dirty_rect result = _dirty_rect;

			_dirty_rect.flush([&] (Rect const &rect) {
				_boundaries.for_each([&] (Boundary const &boundary) {
					Rect const clipped = Rect::intersect(rect, boundary.rect);
					boundary.win_refs.with_first([&] (Win_ref const &first) {
						_draw_rec(canvas, &first, clipped); }); }); });

			return result;
		}

		inline void update_model(Xml_node const &root_node, auto const &flush_fn);

		bool schedule_animated_windows()
		{
			bool redraw_needed = false;

			_for_each_window_const([&] (Window_base const &win) {
				if (win.animated()) {
					_dirty_rect.mark_as_dirty(win.outer_geometry());
					redraw_needed = true;
				}
			});
			return redraw_needed;
		}

		void update_gui_views()
		{
			_boundaries.for_each([&] (Boundary &boundary) {

				/*
				 * Update GUI views in reverse order (back-most first). The
				 * reverse order is important because the stacking position of
				 * a view is propagated by referring to the neighbor the view
				 * is in front of. By starting with the back-most view, we make
				 * sure that each view is always at its final stacking position
				 * when specified as neighbor of another view.
				 */
				Reversed_windows reversed = boundary.reversed_window_list();

				while (Genode::List_element<Window_base> *win = reversed.first()) {
					win->object()->update_gui_views({ boundary.rect });
					reversed.remove(win);
				}
			});
		}

		Window_base::Hover hover(Point pos) const
		{
			Window_base::Hover result { };

			_for_each_window_const([&] (Window_base const &win) {

				if (!result.window_id.value && win.outer_geometry().contains(pos)) {

					Window_base::Hover const hover = win.hover(pos);
					if (hover.window_id.value != 0)
						result = hover;
				}
			});

			return result;
		}


		/**************************************
		 ** Window::Draw_behind_fn interface **
		 **************************************/

		void draw_behind(Canvas_base &canvas, Win_ref const &ref, Rect clip) const override
		{
			_draw_rec(canvas, ref.next(), clip);
		}
};


void Decorator::Window_stack::_draw_rec(Canvas_base &canvas,
                                        Win_ref const *ref, Rect rect) const
{
	Rect clipped;

	/* find next window that intersects with the rectangle */
	for ( ; ref && !(clipped = Rect::intersect(ref->window.outer_geometry(), rect)).valid(); )
		ref = ref->next();

	/* check if we hit the bottom of the window stack */
	if (!ref) return;

	/* draw areas around the current window */
	if (Window_base::Ref const * const next = ref->next()) {

		Rect::Cut_remainder const r = rect.cut(clipped);

		if (r.top.valid())    _draw_rec(canvas, next, r.top);
		if (r.left.valid())   _draw_rec(canvas, next, r.left);
		if (r.right.valid())  _draw_rec(canvas, next, r.right);
		if (r.bottom.valid()) _draw_rec(canvas, next, r.bottom);
	}

	/* draw current window */
	ref->window.draw(canvas, *ref, clipped, *this);
}


void Decorator::Window_stack::update_model(Genode::Xml_node const &root_node,
                                           auto const &flush_window_stack_changes_fn)
{
	Abandoned_boundaries abandoned_boundaries { };
	Abandoned_windows    abandoned_windows    { };

	_boundaries.update_from_xml(root_node,

		[&] (Xml_node const &node) -> Boundary & {
			return *new (_alloc) Boundary(Boundary::name(node)); },

		[&] (Boundary &boundary) {
			boundary.update(_window_factory, abandoned_windows, _dirty_rect,
			                Xml_node("<empty/>"));
			boundary.abandon(abandoned_boundaries);
		},

		[&] (Boundary &boundary, Xml_node const &node) {
			boundary.update(_window_factory, abandoned_windows, _dirty_rect, node); }
	);

	Windows::Id new_front_most_id { ~0UL };

	_boundaries.with_first([&] (Boundary const &boundary) {
		boundary.win_refs.with_first([&] (Window_base::Ref const &ref) {
			new_front_most_id = ref.window.id(); }); });

	/* return true if window just came to front */
	auto new_front_most_window = [&] (Window_base const &win) {
		return (new_front_most_id.value != _front_most_id.value)
		    && (win.id() == new_front_most_id); };

	auto stack_back_most_window = [&] (Window_base &window) {

		if (window.back_most())
			return;

		if (new_front_most_window(window))
			window.stack_front_most();
		else
			window.stack_back_most();

		_dirty_rect.mark_as_dirty(window.outer_geometry());
	};

	auto stack_window = [&] (Window_base &window, Window_base &neighbor) {

		if (window.in_front_of(neighbor))
			return;

		if (new_front_most_window(window))
			window.stack_front_most();
		else
			window.stack(neighbor.frontmost_view());

		_dirty_rect.mark_as_dirty(window.outer_geometry());
	};

	_boundaries.for_each([&] (Boundary &boundary) {

		/*
		 * Propagate changed stacking order to the GUI server
		 *
		 * First, we reverse the window list. The 'reversed' list starts with
		 * the back-most window. We then go throuh each window back to front
		 * and check if its neighbor is consistent with its position in the
		 * window list.
		 */
		Reversed_windows reversed = boundary.reversed_window_list();

		if (Genode::List_element<Window_base> *back_most = reversed.first()) {

			/* handle back-most window */
			reversed.remove(back_most);
			Window_base &window = *back_most->object();
			stack_back_most_window(window);
			window.forget_neighbor();

			Window_base *neighbor = &window;

			/* check consistency between window list order and view stacking */
			while (Genode::List_element<Window_base> *elem = reversed.first()) {

				reversed.remove(elem);

				Window_base &window = *elem->object();
				stack_window(window, *neighbor);
				window.stacking_neighbor(neighbor->frontmost_view());
				neighbor = &window;
			}
		}
	});

	/*
	 * Apply window-creation operations before destroying windows to prevent
	 * flickering.
	 */
	flush_window_stack_changes_fn();

	/*
	 * Destroy abandoned window and boundary objects
	 *
	 * This is done after all other operations to avoid flickering whenever one
	 * window is replaced by another one. If we first destroyed the original
	 * one, the window background would appear for a brief moment until the new
	 * window is created. By deferring the destruction of the old window to the
	 * point when the new one already exists, one of both windows is visible at
	 * all times.
	 */

	abandoned_boundaries.for_each([&] (Boundary &boundary) {
		destroy(_alloc, &boundary); });

	abandoned_windows.for_each([&] (Window_base &window) {
		if (window.referenced()) {
			window.dont_abandon();
		} else {
			Rect const rect = window.outer_geometry();
			_window_factory.destroy_window(window);
			mark_as_dirty(rect);
		}
	});

	_front_most_id = new_front_most_id;
}

#endif /* _INCLUDE__DECORATOR__WINDOW_STACK_H_ */
