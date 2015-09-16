/*
 * \brief  Window-stack handling for decorator
 * \author Norman Feske
 * \date   2014-01-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DECORATOR__WINDOW_STACK_H_
#define _INCLUDE__DECORATOR__WINDOW_STACK_H_

/* Genode includes */
#include <nitpicker_session/nitpicker_session.h>
#include <base/printf.h>

/* local includes */
#include <decorator/types.h>
#include <decorator/window.h>
#include <decorator/window_factory.h>
#include <decorator/xml_utils.h>

namespace Decorator { class Window_stack; }


class Decorator::Window_stack : public Window_base::Draw_behind_fn
{
	private:

		Window_list          _windows;
		Window_factory_base &_window_factory;
		Dirty_rect mutable   _dirty_rect;

		inline void _draw_rec(Canvas_base &canvas, Window_base const *win,
		                      Rect rect) const;

		Window_base *_lookup_by_id(unsigned const id)
		{
			for (Window_base *win = _windows.first(); win; win = win->next())
				if (win->id() == id)
					return win;

			return 0;
		}

		static inline
		Xml_node _xml_node_by_window_id(Genode::Xml_node node, unsigned id)
		{
			for (node = node.sub_node("window"); ; node = node.next()) {

				if (node.has_type("window") && attribute(node, "id", 0UL) == id)
					return node;

				if (node.is_last()) break;
			}

			throw Xml_node::Nonexistent_sub_node();
		}

		void _destroy(Window_base &window)
		{
			_windows.remove(&window);
			_window_factory.destroy(&window);
		}

		/**
		 * Generate window list in reverse order
		 *
		 * After calling this method, the '_windows' list is empty.
		 */
		Window_list _reversed_window_list()
		{
			Window_list reversed;
			while (Window_base *w = _windows.first()) {
				_windows.remove(w);
				reversed.insert(w);
			}
			return reversed;
		}

	public:

		Window_stack(Window_factory_base &window_factory)
		:
			_window_factory(window_factory)
		{ }

		Dirty_rect draw(Canvas_base &canvas) const
		{
			Dirty_rect result = _dirty_rect;

			_dirty_rect.flush([&] (Rect const &rect) {
				_draw_rec(canvas, _windows.first(), rect); });

			return result;
		}

		inline void update_model(Xml_node root_node);

		bool schedule_animated_windows()
		{
			bool redraw_needed = false;

			for (Window_base *win = _windows.first(); win; win = win->next()) {
				if (win->animated()) {
					_dirty_rect.mark_as_dirty(win->outer_geometry());
					redraw_needed = true;
				}
			}
			return redraw_needed;
		}

		/**
		 * Apply functor to each window
		 *
		 * The functor is called with 'Window_base &' as argument.
		 */
		template <typename FUNC>
		void for_each_window(FUNC const &func)
		{
			for (Window_base *win = _windows.first(); win; win = win->next())
				func(*win);
		}

		void update_nitpicker_views()
		{
			/*
			 * Update nitpicker views in reverse order (back-most first). The
			 * reverse order is important because the stacking position of a
			 * view is propagated by referring to the neighbor the view is in
			 * front of. By starting with the back-most view, we make sure that
			 * each view is always at its final stacking position when
			 * specified as neighbor of another view.
			 */
			Window_list reversed = _reversed_window_list();

			while (Window_base *win = reversed.first()) {
				win->update_nitpicker_views();
				reversed.remove(win);
				_windows.insert(win);
			}
		}

		void flush()
		{
			while (Window_base *window = _windows.first())
				_destroy(*window);
		}

		Window_base::Hover hover(Point pos) const
		{
			for (Window_base const *win = _windows.first(); win; win = win->next())
				if (win->outer_geometry().contains(pos))
					return win->hover(pos);

			return Window_base::Hover();
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
		win = win->next();;

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


void Decorator::Window_stack::update_model(Genode::Xml_node root_node)
{
	/*
	 * Step 1: Remove windows that are no longer present.
	 */
	for (Window_base *window = _windows.first(), *next = 0; window; window = next) {
		next = window->next();
		try {
			_xml_node_by_window_id(root_node, window->id());
		}
		catch (Xml_node::Nonexistent_sub_node) {
			_dirty_rect.mark_as_dirty(window->outer_geometry());
			_destroy(*window);
		};
	}

	/*
	 * Step 2: Update window properties of already present windows.
	 */
	for (Window_base *window = _windows.first(); window; window = window->next()) {

		/*
		 * After step 1, a Xml_node::Nonexistent_sub_node exception can no
		 * longer occur. All windows remaining in the window stack are present
		 * in the XML model.
		 */
		try {
			Rect const orig_geometry = window->outer_geometry();
			if (window->update(_xml_node_by_window_id(root_node, window->id()))) {
				_dirty_rect.mark_as_dirty(orig_geometry);
				_dirty_rect.mark_as_dirty(window->outer_geometry());
			}
		}
		catch (Xml_node::Nonexistent_sub_node) {
			PERR("could not look up window %ld in XML model", window->id()); }
	}

	/*
	 * Step 3: Add new appearing windows to the window stack.
	 */
	for_each_sub_node(root_node, "window", [&] (Xml_node window_node) {

		unsigned long const id = attribute(window_node, "id", 0UL);

		if (!_lookup_by_id(id)) {

			Window_base *new_window = _window_factory.create(window_node);

			if (new_window) {

				new_window->update(window_node);

				/*
				 * Insert new window in front of all other windows.
				 *
				 * Immediately propagate the new stacking position of the new
				 * window to nitpicker ('update_nitpicker_views'). Otherwise,
				 */
				new_window->stack(_windows.first()
				                ? _windows.first()->frontmost_view()
				                : Nitpicker::Session::View_handle());

				_windows.insert(new_window);

				_dirty_rect.mark_as_dirty(new_window->outer_geometry());
			}
		}
	});

	/*
	 * Step 4: Adjust window order.
	 */
	Window_base *previous_window = 0;
	Window_base *window          = _windows.first();

	for_each_sub_node(root_node, "window", [&] (Xml_node window_node) {

		if (!window) {
			PERR("unexpected end of window list during re-ordering");
			return;
		}

		unsigned long const id = attribute(window_node, "id", 0UL);

		if (window->id() != id) {
			window = _lookup_by_id(id);
			if (!window) {
				PERR("window lookup unexpectedly failed during re-ordering");
				return;
			}

			_windows.remove(window);
			_windows.insert(window, previous_window);

			_dirty_rect.mark_as_dirty(window->outer_geometry());
		}

		previous_window = window;
		window          = window->next();
	});

	/*
	 * Propagate changed stacking order to nitpicker
	 *
	 * First, we reverse the window list. The 'reversed' list starts with
	 * the back-most window. We then go throuh each window back to front
	 * and check if its neighbor is consistent with its position in the
	 * window list.
	 */
	Window_list reversed = _reversed_window_list();

	if (Window_base * const back_most = reversed.first()) {

		/* keep back-most window as is */
		reversed.remove(back_most);
		_windows.insert(back_most);

		/* check consistency between window list order and view stacking */
		while (Window_base *w = reversed.first()) {

			Window_base * const neighbor = _windows.first();

			reversed.remove(w);
			_windows.insert(w);

			/* propagate change stacking order to nitpicker */
			if (w->is_in_front_of(*neighbor))
				continue;

			w->stack(neighbor->frontmost_view());
		}
	}
}

#endif /* _INCLUDE__DECORATOR__WINDOW_STACK_H_ */
