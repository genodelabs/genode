/*
 * \brief  Nitpicker view stack
 * \author Norman Feske
 * \date   2006-08-08
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW_STACK_H_
#define _VIEW_STACK_H_

#include <view.h>
#include <gui_session.h>
#include <canvas.h>

namespace Nitpicker { class View_stack; }


class Nitpicker::View_stack
{
	public:

		struct Damage : Interface, Noncopyable
		{
			virtual void mark_as_damaged(Rect) = 0;
		};

	private:

		Area                   _size { };
		Focus                 &_focus;
		Font            const &_font;
		List<View_stack_elem>  _views { };
		View                  *_default_background = nullptr;
		Damage                &_damage;

		/**
		 * Return outline geometry of a view
		 *
		 * This geometry depends on the view geometry and the currently active
		 * Nitpicker mode. In non-flat modes, we incorporate the surrounding
		 * frame.
		 */
		Rect _outline(View const &view) const;

		/**
		 * Return top-most view of the view stack
		 */
		View const *_first_view() const
		{
			return static_cast<View const *>(_views.first());
		}

		View *_first_view()
		{
			return static_cast<View *>(_views.first()); 
		}

		/**
		 * Find position in view stack for inserting a view
		 */
		View const *_target_stack_position(View const *neighbor, bool behind);

		/**
		 * Find best visible label position
		 */
		void _optimize_label_rec(View const *cv, View const *lv,
		                         Rect rect, Rect *optimal);

		/**
		 * Position labels that are affected by specified area
		 */
		void _place_labels(Rect);

		/**
		 * Return view following the specified view in the view stack
		 *
		 * The function is a template to capture both const and non-const
		 * usage.
		 */
		template <typename VIEW>
		VIEW *_next_view(VIEW &view) const;

	public:

		/**
		 * Constructor
		 */
		View_stack(Focus &focus, Font const &font, Damage &damage)
		:
			_focus(focus), _font(font), _damage(damage)
		{ }

		/**
		 * Return size
		 */
		Area size() const { return _size; }

		void size(Area size)
		{
			_size = size;

			update_all_views();
		}

		/**
		 * Draw views in specified area (recursivly)
		 *
		 * \param view  current view in view stack
		 */
		void draw_rec(Canvas_base &, Font const &, View const *, Rect) const;

		/**
		 * Draw specified area
		 */
		void draw(Canvas_base &canvas, Rect rect) const
		{
			draw_rec(canvas, _font, _first_view(), rect);
		}

		/**
		 * Trigger redraw of the whole view stack
		 */
		void update_all_views()
		{
			Rect const whole_screen(Point(), _size);

			_place_labels(whole_screen);
			_damage.mark_as_damaged(whole_screen);
		}

		/**
		 * Mark all views belonging to the specified session as dirty
		 *
		 * \param Session  Session that created the view
		 * \param Rect     Buffer area to update
		 */
		void mark_session_views_as_dirty(Gui_session const &session, Rect rect)
		{
			for (View *view = _first_view(); view; view = view->view_stack_next()) {

				if (!view->owned_by(session))
					continue;

				/*
				 * Determine view portion that displays the buffer portion
				 * specified by 'rect'.
				 */
				Point const offset = view->abs_position() + view->buffer_off();
				Rect const r = Rect::intersect(Rect::compound(rect.p1() + offset,
				                                              rect.p2() + offset),
				                               view->abs_geometry());

				refresh_view(*view, r);
			}
		}

		/**
		 * Refresh area within a view
		 *
		 * \param view  view that should be updated on screen
		 */
		void refresh_view(View &view, Rect);

		/**
		 * Refresh entire view
		 */
		void refresh_view(View &view) { refresh_view(view, _outline(view)); }

		/**
		 * Refresh area
		 */
		void refresh(Rect);

		/**
		 * Define view geometry
		 *
		 * \param rect  new geometry of view on screen
		 */
		void geometry(View &view, Rect rect);

		/**
		 * Define buffer offset of view
		 *
		 * \param buffer_off  view offset of displayed buffer
		 */
		void buffer_offset(View &view, Point buffer_off);

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
		void stack(View &view, View const *neighbor = 0,
		           bool behind = true);

		/**
		 * Set view title
		 */
		void title(View &view, Title const &title);

		/**
		 * Find view at specified position
		 */
		View *find_view(Point);

		/**
		 * Remove view from view stack
		 */
		void remove_view(View const &, bool redraw = true);

		/**
		 * Define default background
		 */
		void default_background(View &view) { _default_background = &view; }

		/**
		 * Return true if view is the default background
		 */
		bool is_default_background(View const &view) const
		{
			return &view == _default_background;
		}

		void apply_origin_policy(View &pointer_origin)
		{
			for (View *v = _first_view(); v; v = v->view_stack_next())
				v->apply_origin_policy(pointer_origin);
		}

		void sort_views_by_layer();

		/**
		 * Bring views that match the specified label selector to the front
		 */
		void to_front(char const *selector);
};

#endif /* _VIEW_STACK_H_ */
