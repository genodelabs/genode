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

#include <nitpicker_gfx/canvas.h>
#include <nitpicker_gfx/geometry.h>

#include "list.h"
#include "mode.h"
#include "string.h"
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

	private:

		unsigned _flags;         /* properties of the view            */
		Rect     _label_rect;    /* position and size of label        */
		Point    _buffer_off;    /* offset to the visible buffer area */
		Session *_session;       /* session that created the view     */
		char     _title[TITLE_LEN];

	public:

		enum {
			STAY_TOP    = 0x01,  /* keep view always on top      */
			TRANSPARENT = 0x02,  /* background is partly visible */
			BACKGROUND  = 0x20,  /* view is a background view    */
		};

		View(Session *session, unsigned flags = 0, Rect rect = Rect()):
			Rect(rect), _flags(flags), _session(session) { title(""); }

		virtual ~View() { }

		/**
		 * Return thickness of frame that surrounds the view
		 */
		virtual int frame_size(Mode *mode)
		{
			if (mode->focused_view()
			 && mode->focused_view()->session() == _session)
				return 5;

			return 3;
		}

		/**
		 * Draw view-surrounding frame on canvas
		 */
		virtual void frame(Canvas *canvas, Mode *mode);

		/**
		 * Draw view on canvas
		 */
		virtual void draw(Canvas *canvas, Mode *mode);

		/**
		 * Set view title
		 */
		void title(const char *title);

		/**
		 * Return successor in view stack
		 */
		View *view_stack_next() {
			return static_cast<View *>(View_stack_elem::next()); }

		/**
		 * Set view as background
		 *
		 * \param is_bg  true if view is background
		 */
		void background(bool is_bg)
		{
			if (is_bg) _flags |= BACKGROUND;
			else _flags &= ~BACKGROUND;
		}

		/**
		 * Accessors
		 */
		Session *session()     { return _session; }
		bool     stay_top()    { return _flags & STAY_TOP; }
		bool     transparent() { return _flags & TRANSPARENT || _session->uses_alpha(); }
		bool     background()  { return _flags & BACKGROUND; }
		void     buffer_off(Point buffer_off) { _buffer_off = buffer_off; }
		Point    buffer_off()  { return _buffer_off; }
		Rect     label_rect()  { return _label_rect; }
		void     label_pos(Point pos) { _label_rect = Rect(pos, _label_rect.area()); }
		char    *title()       { return _title; }

		/**
		 * Return true if input at screen position 'p' refers to the view
		 */
		bool input_response_at(Point p, Mode *mode)
		{
			/* check if point lies outside view geometry */
			if ((p.x() < x1()) || (p.x() > x2())
			 || (p.y() < y1()) || (p.y() > y2()))
				return false;

			/* if view uses an alpha channel, check the input mask */
			if (mode->flat() && session()->uses_alpha())
				return session()->input_mask_at(p - p1() + _buffer_off);

			return true;
		}
};


#endif
