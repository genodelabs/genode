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

/* local includes */
#include <decorator/types.h>
#include <decorator/window.h>
#include <decorator/window_factory.h>
#include <decorator/xml_utils.h>

namespace Decorator { class Window_stack; }


class Decorator::Window_stack : public Window_base::Draw_behind_fn
{
	private:

		List_model<Window_base> _windows { };
		Window_factory_base    &_window_factory;
		Dirty_rect mutable      _dirty_rect { };

		unsigned long _front_most_id = ~0UL;

		inline void _draw_rec(Canvas_base &canvas, Window_base const *win,
		                      Rect rect) const;

		static inline
		Xml_node _xml_node_by_window_id(Genode::Xml_node node, unsigned id)
		{
			for (node = node.sub_node("window"); ; node = node.next()) {

				if (node.has_type("window") && node.attribute_value("id", 0UL) == id)
					return node;

				if (node.last()) break;
			}

			throw Xml_node::Nonexistent_sub_node();
		}

		/**
		 * Generate window list in reverse order
		 */
		Reversed_windows _reversed_window_list()
		{
			Reversed_windows reversed { };
			_windows.for_each([&] (Window_base &window) {
				window.prepend_to_reverse_list(reversed); });
			return reversed;
		}

	public:

		Window_stack(Window_factory_base &window_factory)
		:
			_window_factory(window_factory)
		{ }

		void mark_as_dirty(Rect rect) { _dirty_rect.mark_as_dirty(rect); }

		Dirty_rect draw(Canvas_base &canvas) const
		{
			Dirty_rect result = _dirty_rect;

			_dirty_rect.flush([&] (Rect const &rect) {
				_windows.with_first([&] (Window_base const &first) {
					_draw_rec(canvas, &first, rect); }); });

			return result;
		}

		template <typename FN>
		inline void update_model(Xml_node root_node, FN const &flush);

		bool schedule_animated_windows()
		{
			bool redraw_needed = false;

			_windows.for_each([&] (Window_base const &win) {

				if (win.animated()) {
					_dirty_rect.mark_as_dirty(win.outer_geometry());
					redraw_needed = true;
				}
			});
			return redraw_needed;
		}

		/**
		 * Apply functor to each window
		 *
		 * The functor is called with 'Window_base &' as argument.
		 */
		template <typename FUNC>
		void for_each_window(FUNC const &func) { _windows.for_each(func); }

		void update_gui_views()
		{
			/*
			 * Update GUI views in reverse order (back-most first). The
			 * reverse order is important because the stacking position of a
			 * view is propagated by referring to the neighbor the view is in
			 * front of. By starting with the back-most view, we make sure that
			 * each view is always at its final stacking position when
			 * specified as neighbor of another view.
			 */
			Reversed_windows reversed = _reversed_window_list();

			while (Genode::List_element<Window_base> *win = reversed.first()) {
				win->object()->update_gui_views();
				reversed.remove(win);
			}
		}

		Window_base::Hover hover(Point pos) const
		{
			Window_base::Hover result { };

			_windows.for_each([&] (Window_base const &win) {

				if (!result.window_id && win.outer_geometry().contains(pos)) {

					Window_base::Hover const hover = win.hover(pos);
					if (hover.window_id != 0)
						result = hover;
				}
			});

			return result;
		}


		/**************************************
		 ** Window::Draw_behind_fn interface **
		 **************************************/

		void draw_behind(Canvas_base &canvas, Window_base const &window, Rect clip) const override
		{
			_draw_rec(canvas, window.next(), clip);
		}
};


void Decorator::Window_stack::_draw_rec(Decorator::Canvas_base       &canvas,
                                        Decorator::Window_base const *win,
                                        Decorator::Rect               rect) const
{
	Rect clipped;

	/* find next window that intersects with the rectangle */
	for ( ; win && !(clipped = Rect::intersect(win->outer_geometry(), rect)).valid(); )
		win = win->next();

	/* check if we hit the bottom of the window stack */
	if (!win) return;

	/* draw areas around the current window */
	if (Window_base const * const next = win->next()) {
		Rect top, left, right, bottom;
		rect.cut(clipped, &top, &left, &right, &bottom);

		if (top.valid())    _draw_rec(canvas, next, top);
		if (left.valid())   _draw_rec(canvas, next, left);
		if (right.valid())  _draw_rec(canvas, next, right);
		if (bottom.valid()) _draw_rec(canvas, next, bottom);
	}

	/* draw current window */
	win->draw(canvas, clipped, *this);
}


template <typename FN>
void Decorator::Window_stack::update_model(Genode::Xml_node root_node,
                                           FN const &flush_window_stack_changes)
{
	Abandoned_windows _abandoned_windows { };

	_windows.update_from_xml(root_node,

		[&] (Xml_node const &node) -> Window_base & {
			return *_window_factory.create(node); },

		[&] (Window_base &window) { window.abandon(_abandoned_windows); },

		[&] (Window_base &window, Xml_node const &node)
		{
			Rect const orig_geometry = window.outer_geometry();

			if (window.update(node)) {
				_dirty_rect.mark_as_dirty(orig_geometry);
				_dirty_rect.mark_as_dirty(window.outer_geometry());
			}
		}
	);

	unsigned long new_front_most_id = ~0UL;
	if (root_node.has_sub_node("window"))
		new_front_most_id = root_node.sub_node("window").attribute_value("id", ~0UL);

	/*
	 * Propagate changed stacking order to the GUI server
	 *
	 * First, we reverse the window list. The 'reversed' list starts with
	 * the back-most window. We then go throuh each window back to front
	 * and check if its neighbor is consistent with its position in the
	 * window list.
	 */
	Reversed_windows reversed = _reversed_window_list();

	/* return true if window just came to front */
	auto new_front_most_window = [&] (Window_base const &win) {
		return (new_front_most_id != _front_most_id) && (win.id() == new_front_most_id); };

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

	if (Genode::List_element<Window_base> *back_most = reversed.first()) {

		/* handle back-most window */
		reversed.remove(back_most);
		Window_base &window = *back_most->object();
		stack_back_most_window(window);
		window.stacking_neighbor(Window_base::View_handle());

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

	/*
	 * Apply window-creation operations before destroying windows to prevent
	 * flickering.
	 */
	flush_window_stack_changes();

	/*
	 * Destroy abandoned window objects
	 *
	 * This is done after all other operations to avoid flickering whenever one
	 * window is replaced by another one. If we first destroyed the original
	 * one, the window background would appear for a brief moment until the new
	 * window is created. By deferring the destruction of the old window to the
	 * point when the new one already exists, one of both windows is visible at
	 * all times.
	 */
	Genode::List_element<Window_base> *elem = _abandoned_windows.first(), *next = nullptr;
	for (; elem; elem = next) {
		next = elem->next();
		_dirty_rect.mark_as_dirty(elem->object()->outer_geometry());
		_window_factory.destroy(elem->object());
	}

	_front_most_id = new_front_most_id;
}

#endif /* _INCLUDE__DECORATOR__WINDOW_STACK_H_ */
