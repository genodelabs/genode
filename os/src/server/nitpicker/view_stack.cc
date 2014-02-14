/*
 * \brief  Nitpicker view stack implementation
 * \author Norman Feske
 * \date   2006-08-09
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "view_stack.h"
#include "clip_guard.h"


/***************
 ** Utilities **
 ***************/

/**
 * Get last view with the stay_top attribute
 *
 * \param view  first view of the view stack
 */
static View const *last_stay_top_view(View const *view)
{
	for (; view && view->view_stack_next() && view->view_stack_next()->stay_top(); )
		view = view->view_stack_next();

	return view;
}


/**************************
 ** View stack interface **
 **************************/

template <typename VIEW>
VIEW *View_stack::_next_view(VIEW *view) const
{
	Session * const active_session = _mode.focused_view() ?
	                                &_mode.focused_view()->session() : 0;

	View * const active_background = active_session ?
	                                 active_session->background() : 0;

	for (;;) {
		view = view ? view->view_stack_next() : 0;

		/* check if we hit the bottom of the view stack */
		if (!view) return 0;

		if (!view->background()) return view;

		if (is_default_background(view) || view == active_background)
			return view;

		/* view is a background view belonging to a non-focused session */
	}
	return 0;
}


Rect View_stack::_outline(View const &view) const
{
	Rect const rect = view.abs_geometry();

	if (_mode.flat()) return rect;

	/* request thickness of view frame */
	int const frame_size = view.frame_size(_mode);

	return Rect(Point(rect.x1() - frame_size, rect.y1() - frame_size),
	            Point(rect.x2() + frame_size, rect.y2() + frame_size));
}


View const *View_stack::_target_stack_position(View const *neighbor, bool behind)
{
	/* find target position in view stack */
	View const *cv = last_stay_top_view(_first_view());

	for ( ; cv; cv = _next_view(cv)) {

		/* bring view to front? */
		if (behind && !neighbor)
			break;

		/* insert view after cv? */
		if (behind && (cv == neighbor))
			break;

		/* insert view in front of cv? */
		if (!behind && (_next_view(cv) == neighbor))
			break;

		/* insert view in front of the background? */
		if (!behind && !neighbor && _next_view(cv)->background())
			break;
	}

	return cv ? cv : last_stay_top_view(_first_view());
}


void View_stack::_optimize_label_rec(View const *cv, View const *lv, Rect rect, Rect *optimal)
{
	/* if label already fits in optimized rectangle, we are happy */
	if (optimal->fits(lv->label_rect().area()))
		return;

	/* find next view that intersects with the rectangle or the target view */
	Rect clipped;
	while (cv && cv != lv && !(clipped = Rect::intersect(_outline(*cv), rect)).valid())
		cv = _next_view(cv);

	/* reached end of view stack */
	if (!cv) return;

	if (cv != lv && _next_view(cv)) {

		/* cut current view from rectangle and go into sub rectangles */
		Rect r[4];
		rect.cut(clipped, &r[0], &r[1], &r[2], &r[3]);
		for (int i = 0; i < 4; i++)
			_optimize_label_rec(_next_view(cv), lv, r[i], optimal);

		return;
	}

	/*
	 * Now, cv equals lv and we must decide how to configure the
	 * optimal rectangle.
	 */

	/* stop if label does not fit vertically */
	if (rect.h() < lv->label_rect().h())
		return;

	/*
	 * If label fits completely within current rectangle, we are done.
	 * If label's width is not fully visible, choose the widest rectangle.
	 */
	if (rect.fits(lv->label_rect().area()) || (rect.w() > optimal->w())) {
		*optimal = rect;
		return;
	}
}


void View_stack::_place_labels(Canvas_base &canvas, Rect rect)
{
	/* do not calculate label positions in flat mode */
	if (_mode.flat()) return;

	/* ignore mouse cursor */
	View const *start = _next_view(_first_view());
	View       *view  = _next_view(_first_view());

	for (; view && _next_view(view); view = _next_view(view)) {

		Rect const view_rect = view->abs_geometry();
		if (Rect::intersect(view_rect, rect).valid()) {

			Rect old = view->label_rect(), best;

			/* calculate best visible label position */
			Rect rect = Rect::intersect(Rect(Point(), canvas.size()), view_rect);
			if (start) _optimize_label_rec(start, view, rect, &best);

			/*
			 * If label is not fully visible, we ensure to display the first
			 * (most important) part. Otherwise, we center the label horizontally.
			 */
			int x = best.x1();
			if (best.fits(view->label_rect().area()))
				x += (best.w() - view->label_rect().w()) / 2;

			view->label_pos(Point(x, best.y1()));

			/* refresh old and new label positions */
			refresh_view(canvas, *view, view, old);
			refresh_view(canvas, *view, view, view->label_rect());
		}
	}
}


void View_stack::draw_rec(Canvas_base &canvas, View const *view, View const *dst_view,
                          Session const *exclude, Rect rect) const
{
	Rect clipped;

	/* find next view that intersects with the current clipping rectangle */
	for ( ; view && !(clipped = Rect::intersect(_outline(*view), rect)).valid(); )
		view = _next_view(view);

	/* check if we hit the bottom of the view stack */
	if (!view) return;

	Rect top, left, right, bottom;
	rect.cut(clipped, &top, &left, &right, &bottom);

	View const *next = _next_view(view);

	/* draw areas at the top/left of the current view */
	if (next && top.valid())  draw_rec(canvas, next, dst_view, exclude, top);
	if (next && left.valid()) draw_rec(canvas, next, dst_view, exclude, left);

	/* draw current view */
	if (!dst_view || (dst_view == view) || view->transparent()) {

		Clip_guard clip_guard(canvas, clipped);

		/* draw background if view is transparent */
		if (view->uses_alpha())
			draw_rec(canvas, _next_view(view), 0, 0, clipped);

		view->frame(canvas, _mode);

		if (!exclude || !view->belongs_to(*exclude))
			view->draw(canvas, _mode);
	}

	/* draw areas at the bottom/right of the current view */
	if (next &&  right.valid()) draw_rec(canvas, next, dst_view, exclude, right);
	if (next && bottom.valid()) draw_rec(canvas, next, dst_view, exclude, bottom);
}


void View_stack::refresh_view(Canvas_base &canvas, View const &view,
                              View const *dst_view, Rect rect)
{
	/* clip argument agains view outline */
	rect = Rect::intersect(rect, _outline(view));

	draw_rec(canvas, _first_view(), dst_view, 0, rect);
}


void View_stack::viewport(Canvas_base &canvas, View &view, Rect rect,
                          Point buffer_off, bool do_redraw)
{
	Rect const old_compound = _compound_outline(view);

	view.geometry(Rect(rect));
	view.buffer_off(buffer_off);

	Rect const new_compound = _compound_outline(view);

	Rect const compound = Rect::compound(old_compound, new_compound);

	/* update labels (except when moving the mouse cursor) */
	if (&view != _first_view())
		_place_labels(canvas, compound);

	if (!_mode.flat())
		do_redraw = true;

	/* update area on screen */
	draw_rec(canvas, _first_view(), 0, do_redraw ? 0 : &view.session(), compound);
}


void View_stack::stack(Canvas_base &canvas, View const &view,
                       View const *neighbor, bool behind, bool do_redraw)
{
	_views.remove(&view);
	_views.insert(&view, _target_stack_position(neighbor, behind));

	_place_labels(canvas, view.abs_geometry());

	/* refresh affected screen area */
	refresh_view(canvas, view, 0, _outline(view));
}


void View_stack::title(Canvas_base &canvas, View &view, const char *title)
{
	view.title(title);
	_place_labels(canvas, view.abs_geometry());
	refresh_view(canvas, view, 0, _outline(view));
}


View *View_stack::find_view(Point p)
{
	/* skip mouse cursor */
	View *view = _next_view(_first_view());

	for ( ; view; view = _next_view(view))
		if (view->input_response_at(p, _mode))
			return view;

	return 0;
}


void View_stack::remove_view(Canvas_base &canvas, View const &view, bool redraw)
{
	/* remember geometry of view to remove */
	Rect rect = _outline(view);

	/* exclude view from view stack */
	_views.remove(&view);

	/*
	 * Reset focused and pointed-at view if necessary
	 *
	 * Thus must be done after calling '_views.remove' because the new focused
	 * pointer is determined by traversing the view stack. If the to-be-removed
	 * view would still be there, we would re-assign the old pointed-to view as
	 * the current one, resulting in a dangling pointer right after the view
	 * gets destructed by the caller of 'removed_view'.
	 */
	_mode.forget(view);

	/* redraw area where the view was visible */
	if (redraw)
		draw_rec(canvas, _first_view(), 0, 0, rect);
}
