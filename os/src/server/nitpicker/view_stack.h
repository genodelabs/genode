/*
 * \brief  Nitpicker view stack
 * \author Norman Feske
 * \date   2006-08-08
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VIEW_STACK_H_
#define _VIEW_STACK_H_

#include "view.h"
#include "canvas.h"

class Session;

class View_stack
{
	private:

		Area                           _size;
		Mode                          &_mode;
		Genode::List<View_stack_elem>  _views;
		View                          *_default_background;

		/**
		 * Return outline geometry of a view
		 *
		 * This geometry depends on the view geometry and the currently
		 * active Nitpicker mode. In non-flat modes, we incorporate the
		 * surrounding frame.
		 */
		Rect _outline(View const &view) const;

		/**
		 * Return top-most view of the view stack
		 */
		View *_first_view() { return static_cast<View *>(_views.first()); }

		/**
		 * Find position in view stack for inserting a view
		 */
		View const *_target_stack_position(View const *neighbor, bool behind);

		/**
		 * Find best visible label position
		 */
		void _optimize_label_rec(View const *cv, View const *lv, Rect rect, Rect *optimal);

		/**
		 * Position labels that are affected by specified area
		 */
		void _place_labels(Canvas_base &, Rect);

		/**
		 * Return compound rectangle covering the view and all of its children
		 */
		Rect _compound_outline(View const &view)
		{
			Rect rect = _outline(view);

			view.for_each_child([&] (View const &child) {
			 	rect = Rect::compound(_outline(child), rect); });

			return rect;
		}

		/**
		 * Return view following the specified view in the view stack
		 *
		 * The function is a template to capture both const and non-const
		 * usage.
		 */
		template <typename VIEW>
		VIEW *_next_view(VIEW *view) const;

	public:

		/**
		 * Constructor
		 */
		View_stack(Area size, Mode &mode) :
			_size(size), _mode(mode), _default_background(0) { }

		/**
		 * Return size
		 */
		Area size() const { return _size; }

		void size(Area size) { _size = size; }

		/**
		 * Draw views in specified area (recursivly)
		 *
		 * \param view      current view in view stack
		 * \param dst_view  desired view to draw or NULL
		 *                  if all views should be drawn
		 * \param exclude   do not draw views of this session
		 */
		void draw_rec(Canvas_base &, View const *view, View const *dst_view, Session const *exclude, Rect) const;

		/**
		 * Draw whole view stack
		 */
		void update_all_views(Canvas_base &canvas)
		{
			_place_labels(canvas, Rect(Point(), _size));
			draw_rec(canvas, _first_view(), 0, 0, Rect(Point(), _size));
		}

		/**
		 * Update all views belonging to the specified session
		 *
		 * \param Session  Session that created the view
		 * \param Rect     Buffer area to update
		 *
		 * Note: For now, we perform an independent view-stack traversal
		 *       for each view when calling 'refresh_view'. This becomes
		 *       a potentially high overhead with many views. Having
		 *       a tailored 'draw_rec_session' function would overcome
		 *       this problem.
		 */
		void update_session_views(Canvas_base &canvas, Session const &session, Rect rect)
		{
			for (View const *view = _first_view(); view; view = view->view_stack_next()) {

				if (!view->belongs_to(session))
					continue;

				/*
				 * Determine view portion that displays the buffer portion
				 * specified by 'rect'.
				 */
				Point const offset = view->abs_position() + view->buffer_off();
				Rect const r = Rect::intersect(Rect(rect.p1() + offset,
				                                    rect.p2() + offset),
				                               view->abs_geometry());
				refresh_view(canvas, *view, view, r);
			}
		}

		/**
		 * Refresh area within a view
		 *
		 * \param view  view that should be updated on screen
		 * \param dst   NULL if all views in the specified area should be
		 *              refreshed or 'view' if the refresh should be limited to
		 *              the specified view.
		 */
		void refresh_view(Canvas_base &, View const &view, View const *dst, Rect);

		/**
		 * Define position and viewport
		 *
		 * \param pos         position of view on screen
		 * \param buffer_off  view offset of displayed buffer
		 * \param do_redraw   perform screen update immediately
		 */
		void viewport(Canvas_base &, View &view, Rect pos, Point buffer_off, bool do_redraw);

		/**
		 * Insert view at specified position in view stack
		 *
		 * \param behind  insert view in front (true) or
		 *                behind (false) the specified neighbor
		 *
		 * To insert a view at the top of the view stack, specify
		 * neighbor = 0 and behind = true. To insert a view at the
		 * bottom of the view stack, specify neighbor = 0 and
		 * behind = false.
		 */
		void stack(Canvas_base &, View const &view, View const *neighbor = 0,
		           bool behind = true, bool do_redraw = true);

		/**
		 * Set view title
		 */
		void title(Canvas_base &, View &view, char const *title);

		/**
		 * Find view at specified position
		 */
		View *find_view(Point);

		/**
		 * Remove view from view stack
		 */
		void remove_view(Canvas_base &, View const &, bool redraw = true);

		/**
		 * Define default background
		 */
		void default_background(View &view) { _default_background = &view; }

		/**
		 * Return true if view is the default background
		 */
		bool is_default_background(View const *view) const { return view == _default_background; }

		/**
		 * Remove all views of specified session from view stack
		 *
		 * Rather than removing the views from the view stack, this function moves
		 * the session views out of the visible screen area.
		 */
		void lock_out_session(Canvas_base &canvas, Session const &session)
		{
			View const *view = _first_view(), *next_view = view->view_stack_next();
			while (view) {
				if (view->belongs_to(session)) remove_view(canvas, *view);
				view = next_view;
				next_view = view ? view->view_stack_next() : 0;
			}
		}
};

#endif
