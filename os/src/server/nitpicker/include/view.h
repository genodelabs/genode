/*
 * \brief  Nitpicker view interface
 * \author Norman Feske
 * \date   2006-08-08
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VIEW_H_
#define _VIEW_H_

#include <util/string.h>
#include <nitpicker_gfx/canvas.h>
#include <nitpicker_gfx/geometry.h>

#include "list.h"
#include "mode.h"
#include "session.h"

class Buffer;


/*
 * For each buffer, there is a list of views that belong to
 * this buffer. This list is called Same_buffer_list.
 */
class Same_buffer_list_elem : public List<Same_buffer_list_elem>::Element { };


class View_stack_elem : public List<View_stack_elem>::Element { };


class View : public Same_buffer_list_elem, public View_stack_elem, public Rect
{
	public:

		enum { TITLE_LEN = 32 };   /* max.characters of a title */

		enum Stay_top    { NOT_STAY_TOP    = 0, STAY_TOP    = 1 };
		enum Transparent { NOT_TRANSPARENT = 0, TRANSPARENT = 1 };
		enum Background  { NOT_BACKGROUND  = 0, BACKGROUND  = 1 };

	private:

		Stay_top    const _stay_top;      /* keep view always on top      */
		Transparent const _transparent;   /* background is partly visible */
		Background        _background;    /* view is a background view    */

		Rect     _label_rect;    /* position and size of label        */
		Point    _buffer_off;    /* offset to the visible buffer area */
		Session &_session;       /* session that created the view     */
		char     _title[TITLE_LEN];

	public:

		View(Session &session, Stay_top stay_top, Transparent transparent,
		     Background background, Rect rect)
		:
			Rect(rect), _stay_top(stay_top), _transparent(transparent),
			_background(background), _session(session)
		{ title(""); }

		virtual ~View() { }

		/**
		 * Return thickness of frame that surrounds the view
		 */
		virtual int frame_size(Mode const &mode) const
		{
			if (mode.focused_view()
			 && mode.focused_view()->belongs_to(_session))
				return 5;

			return 3;
		}

		/**
		 * Draw view-surrounding frame on canvas
		 */
		virtual void frame(Canvas &canvas, Mode const &mode) const;

		/**
		 * Draw view on canvas
		 */
		virtual void draw(Canvas &canvas, Mode const &mode) const;

		/**
		 * Set view title
		 */
		void title(const char *title);

		/**
		 * Return successor in view stack
		 */
		View const *view_stack_next() const {
			return static_cast<View const *>(View_stack_elem::next()); }

		View *view_stack_next() {
			return static_cast<View *>(View_stack_elem::next()); }

		/**
		 * Set view as background
		 *
		 * \param is_bg  true if view is background
		 */
		void background(bool is_bg) {
			_background = is_bg ? BACKGROUND : NOT_BACKGROUND; }

		/**
		 * Accessors
		 */
		Session &session() const { return _session; }

		bool belongs_to(Session const &session) const { return &session == &_session; }
		bool same_session_as(View const &other) const { return &_session == &other._session; }

		bool  stay_top()    const { return _stay_top; }
		bool  transparent() const { return _transparent || _session.uses_alpha(); }
		bool  background()  const { return _background; }
		Point buffer_off()  const { return _buffer_off; }
		Rect  label_rect()  const { return _label_rect; }
		bool  uses_alpha()  const { return _session.uses_alpha(); }

		char const *title() const { return _title; }

		void  buffer_off(Point buffer_off) { _buffer_off = buffer_off; }

		void  label_pos(Point pos) { _label_rect = Rect(pos, _label_rect.area()); }

		/**
		 * Return true if input at screen position 'p' refers to the view
		 */
		bool input_response_at(Point p, Mode const &mode) const
		{
			/* check if point lies outside view geometry */
			if ((p.x() < x1()) || (p.x() > x2())
			 || (p.y() < y1()) || (p.y() > y2()))
				return false;

			/* if view uses an alpha channel, check the input mask */
			if (mode.flat() && session().uses_alpha())
				return session().input_mask_at(p - p1() + _buffer_off);

			return true;
		}
};

#endif /* _VIEW_H_ */
