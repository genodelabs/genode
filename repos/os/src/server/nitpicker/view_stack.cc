/*
 * \brief  Nitpicker view stack implementation
 * \author Norman Feske
 * \date   2006-08-09
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "view_stack.h"
#include "clip_guard.h"


/**************************
 ** View stack interface **
 **************************/

template <typename VIEW>
VIEW *View_stack::_next_view(VIEW &view) const
{
	Session * const focused_session = _mode.focused_session();

	View * const active_background = focused_session ?
	                                 focused_session->background() : 0;

	for (VIEW *next_view = &view; ;) {

		next_view = next_view->view_stack_next();

		/* check if we hit the bottom of the view stack */
		if (!next_view) return 0;

		if (!next_view->session().visible()) continue;

		if (!next_view->background()) return next_view;

		if (is_default_background(*next_view) || next_view == active_background)
			return next_view;

		/* view is a background view belonging to a non-focused session */
	}
	return 0;
}


Rect View_stack::_outline(View const &view) const
{
	Rect const rect = view.abs_geometry();

	/* request thickness of view frame */
	int const frame_size = view.frame_size(_mode);

	return Rect(Point(rect.x1() - frame_size, rect.y1() - frame_size),
	            Point(rect.x2() + frame_size, rect.y2() + frame_size));
}


View const *View_stack::_target_stack_position(View const *neighbor, bool behind)
{
	if (behind) {

		if (!neighbor)
			return nullptr;

		/* find target position behind neighbor */
		for (View const *cv = _first_view(); cv; cv = _next_view(*cv))
			if (cv == neighbor)
				return cv;

	} else {

		if (neighbor == _first_view())
			return nullptr;

		/* find target position in front of neighbor */
		for (View const *cv = _first_view(), *next = nullptr; cv; cv = next) {

			next = _next_view(*cv);
			if (!next || next == neighbor || next->background())
				return cv;
		}
	}

	/* we should never reach this point */
	return nullptr;
}


void View_stack::_optimize_label_rec(View const *cv, View const *lv, Rect rect, Rect *optimal)
{
	/* if label already fits in optimized rectangle, we are happy */
	if (optimal->fits(lv->label_rect().area()))
		return;

	/* find next view that intersects with the rectangle or the target view */
	Rect clipped;
	while (cv && cv != lv && !(clipped = Rect::intersect(_outline(*cv), rect)).valid())
		cv = _next_view(*cv);

	/* reached end of view stack */
	if (!cv) return;

	if (cv != lv && _next_view(*cv)) {

		/* cut current view from rectangle and go into sub rectangles */
		Rect r[4];
		rect.cut(clipped, &r[0], &r[1], &r[2], &r[3]);
		for (int i = 0; i < 4; i++)
			_optimize_label_rec(_next_view(*cv), lv, r[i], optimal);

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


void View_stack::_place_labels(Rect rect)
{
	/*
	 * XXX We may skip this if none of the domains have the labeling enabled.
	 */

	/* ignore mouse cursor */
	View const *start = _next_view(*_first_view());
	View       *view  = _next_view(*_first_view());

	for (; view && _next_view(*view); view = _next_view(*view)) {

		Rect const view_rect = view->abs_geometry();
		if (Rect::intersect(view_rect, rect).valid()) {

			Rect old = view->label_rect(), best;

			/* calculate best visible label position */
			Rect rect = Rect::intersect(Rect(Point(), _size), view_rect);
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
			refresh_view(*view, old);
			refresh_view(*view, view->label_rect());
		}
	}
}


void View_stack::draw_rec(Canvas_base &canvas, View const *view, Rect rect) const
{
	Rect clipped;

	/* find next view that intersects with the current clipping rectangle */
	for ( ; view && !(clipped = Rect::intersect(_outline(*view), rect)).valid(); )
		view = _next_view(*view);

	/* check if we hit the bottom of the view stack */
	if (!view) return;

	Rect top, left, right, bottom;
	rect.cut(clipped, &top, &left, &right, &bottom);

	View const *next = _next_view(*view);

	/* draw areas at the top/left of the current view */
	if (next &&  top.valid()) draw_rec(canvas, next, top);
	if (next && left.valid()) draw_rec(canvas, next, left);

	/* draw current view */
	view->dirty_rect().flush([&] (Rect const &dirty_rect) {

		Clip_guard clip_guard(canvas, Rect::intersect(clipped, dirty_rect));

		/* draw background if view is transparent */
		if (view->uses_alpha())
			draw_rec(canvas, _next_view(*view), clipped);

		view->frame(canvas, _mode);
		view->draw(canvas, _mode);
	});

	/* draw areas at the bottom/right of the current view */
	if (next &&  right.valid()) draw_rec(canvas, next, right);
	if (next && bottom.valid()) draw_rec(canvas, next, bottom);
}


void View_stack::refresh_view(View &view, Rect const rect)
{
	/* rectangle constrained to view geometry */
	Rect const view_rect = Rect::intersect(rect, _outline(view));

	for (View *v = _first_view(); v; v = v->view_stack_next()) {

		Rect const intersection = Rect::intersect(view_rect, _outline(*v));

		if (intersection.valid())
			_mark_view_as_dirty(*v, intersection);
	}

	view.for_each_child([&] (View &child) { refresh_view(child, rect); });
}


void View_stack::refresh(Rect const rect)
{
	for (View *v = _first_view(); v; v = v->view_stack_next()) {

		Rect const intersection = Rect::intersect(rect, _outline(*v));

		if (intersection.valid())
			refresh_view(*v, intersection);
	}
}


void View_stack::geometry(View &view, Rect const rect)
{
	Rect const old_outline = _outline(view);

	/*
	 * Refresh area covered by the original view geometry.
	 *
	 * We specify the whole geometry to also cover the refresh of child
	 * views. The 'refresh_view' function takes care to constrain the
	 * refresh to the actual view geometry.
	 */
	refresh_view(view, Rect(Point(), _size));

	/* change geometry */
	view.geometry(Rect(rect));

	/* refresh new view geometry */
	refresh_view(view, Rect(Point(), _size));

	Rect const compound = Rect::compound(old_outline, _outline(view));

	/* update labels (except when moving the mouse cursor) */
	if (&view != _first_view())
		_place_labels(compound);
}


void View_stack::buffer_offset(View &view, Point const buffer_off)
{
	view.buffer_off(buffer_off);

	refresh_view(view, Rect(Point(), _size));
}


void View_stack::stack(View &view, View const *neighbor, bool behind)
{
	_views.remove(&view);
	_views.insert(&view, _target_stack_position(neighbor, behind));

	/* enforce stacking constrains dictated by domain layers */
	sort_views_by_layer();

	_place_labels(view.abs_geometry());

	refresh_view(view, _outline(view));
}


void View_stack::title(View &view, const char *title)
{
	view.title(title);
	_place_labels(view.abs_geometry());

	_mark_view_as_dirty(view, _outline(view));
}


View *View_stack::find_view(Point p)
{
	View *view = _first_view();

	for ( ; view; view = _next_view(*view))
		if (view->input_response_at(p, _mode))
			return view;

	return 0;
}


void View_stack::remove_view(View const &view, bool redraw)
{
	view.for_each_const_child([&] (View const &child) { remove_view(child); });

	/* remember geometry of view to remove */
	Rect rect = _outline(view);

	/* exclude view from view stack */
	_views.remove(&view);

	refresh(rect);
}


void View_stack::sort_views_by_layer()
{
	Genode::List<View_stack_elem> sorted;

	/* last element of the sorted list */
	View_stack_elem *at = nullptr;

	while (_views.first()) {

		/* find view with the lowest layer */
		unsigned         lowest_layer = ~0U;
		View_stack_elem *lowest_view  = nullptr;
		for (View_stack_elem *v = _views.first(); v; v = v->next()) {
			unsigned const layer = static_cast<View *>(v)->session().layer();
			if (layer < lowest_layer) {
				lowest_layer = layer;
				lowest_view  = v;
			}
		}

		if (!lowest_view)
			lowest_view = _views.first();

		/*
		 * Move lowest view from unsorted list to the end of the sorted
		 * list.
		 */
		_views.remove(lowest_view);
		sorted.insert(lowest_view, at);
		at = lowest_view;
	}

	/* replace empty source list by newly sorted list */
	_views = sorted;
}
