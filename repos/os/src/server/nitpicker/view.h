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
#include <util/list.h>
#include <util/dirty_rect.h>

#include "mode.h"
#include "session.h"

class Buffer;

#include <framebuffer_session/framebuffer_session.h>
extern Framebuffer::Session *tmp_fb;


typedef Genode::Dirty_rect<Rect, 3> Dirty_rect;


/*
 * For each buffer, there is a list of views that belong to
 * this buffer. This list is called Same_buffer_list.
 */
struct Same_buffer_list_elem : Genode::List<Same_buffer_list_elem>::Element { };


struct View_stack_elem : Genode::List<View_stack_elem>::Element { };


/*
 * If a view has a parent, it is a list element of its parent view
 */
struct View_parent_elem : Genode::List<View_parent_elem>::Element { };


class View : public Same_buffer_list_elem,
             public View_stack_elem,
             public View_parent_elem
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

		View      *_parent;         /* parent view                          */
		Rect       _geometry;       /* position and size relative to parent */
		Rect       _label_rect;     /* position and size of label           */
		Point      _buffer_off;     /* offset to the visible buffer area    */
		Session   &_session;        /* session that created the view        */
		char       _title[TITLE_LEN];

		Genode::List<View_parent_elem> _children;

	public:

		View(Session &session, Stay_top stay_top, Transparent transparent,
		     Background bg, View *parent)
		:
			_stay_top(stay_top), _transparent(transparent), _background(bg),
			_parent(parent), _session(session)
		{ title(""); }

		virtual ~View()
		{
			/* break link to our parent */
			if (_parent)
				_parent->remove_child(this);

			/* break links to our children */
			while (View_parent_elem *e = _children.first())
				static_cast<View *>(e)->dissolve_from_parent();
		}

		/**
		 * Return absolute view position
		 */
		Point abs_position() const
		{
			return _parent ? _geometry.p1() + _parent->abs_position()
			               : _geometry.p1();
		}

		/**
		 * Return absolute view geometry
		 */
		Rect abs_geometry() const
		{
			return Rect(abs_position(), _geometry.area());
		}

		/**
		 * Break the relationship of a child view from its parent
		 *
		 * This function is called when a parent view gets destroyed.
		 */
		void dissolve_from_parent()
		{
			_parent   = 0;
			_geometry = Rect();
		}

		Rect geometry() const { return _geometry; }

		void geometry(Rect geometry) { _geometry = geometry; }

		void add_child(View const *child) { _children.insert(child); }

		void remove_child(View const *child) { _children.remove(child); }

		template <typename FN>
		void for_each_child(FN const &fn) const {
			for (View_parent_elem const *e = _children.first(); e; e = e->next())
				fn(*static_cast<View const *>(e));
		}

		/**
		 * Return thickness of frame that surrounds the view
		 */
		virtual int frame_size(Mode const &mode) const
		{
			return mode.is_focused(_session) ? 5 : 3;
		}

		/**
		 * Draw view-surrounding frame on canvas
		 */
		virtual void frame(Canvas_base &canvas, Mode const &mode) const;

		/**
		 * Draw view on canvas
		 */
		virtual void draw(Canvas_base &canvas, Mode const &mode) const;

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

		bool  top_level()   const { return _parent == 0; }
		bool  stay_top()    const { return _stay_top; }
		bool  transparent() const { return _transparent || _session.uses_alpha(); }
		bool  background()  const { return _background; }
		Rect  label_rect()  const { return _label_rect; }
		bool  uses_alpha()  const { return _session.uses_alpha(); }
		Point buffer_off()  const { return _buffer_off; }

		char const *title() const { return _title; }

		void buffer_off(Point buffer_off) { _buffer_off = buffer_off; }

		void label_pos(Point pos) { _label_rect = Rect(pos, _label_rect.area()); }

		/**
		 * Return true if input at screen position 'p' refers to the view
		 */
		bool input_response_at(Point p, Mode const &mode) const
		{
			Rect const view_rect = abs_geometry();

			/* check if point lies outside view geometry */
			if ((p.x() < view_rect.x1()) || (p.x() > view_rect.x2())
			 || (p.y() < view_rect.y1()) || (p.y() > view_rect.y2()))
				return false;

			/* if view uses an alpha channel, check the input mask */
			if (mode.flat() && session().uses_alpha())
				return session().input_mask_at(p - view_rect.p1() + _buffer_off);

			return true;
		}
};

#endif /* _VIEW_H_ */
