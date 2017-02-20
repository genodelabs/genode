/*
 * \brief  Nitpicker view interface
 * \author Norman Feske
 * \date   2006-08-08
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW_H_
#define _VIEW_H_

/* Genode includes */
#include <util/string.h>
#include <util/list.h>
#include <util/dirty_rect.h>
#include <base/weak_ptr.h>
#include <base/rpc_server.h>

/* local includes */
#include "mode.h"
#include "session.h"

class Buffer;

#include <framebuffer_session/framebuffer_session.h>
extern Framebuffer::Session *tmp_fb;


typedef Genode::Dirty_rect<Rect, 3> Dirty_rect;


/*
 * For each buffer, there is a list of views that belong to this buffer.
 */
struct Same_buffer_list_elem : Genode::List<Same_buffer_list_elem>::Element { };

/*
 * The view stack holds a list of all visible view in stacking order.
 */
struct View_stack_elem : Genode::List<View_stack_elem>::Element { };

/*
 * Each session maintains a list of views owned by the session.
 */
struct Session_view_list_elem : Genode::List<Session_view_list_elem>::Element { };

/*
 * If a view has a parent, it is a list element of its parent view
 */
struct View_parent_elem : Genode::List<View_parent_elem>::Element { };


namespace Nitpicker { class View; }


/*
 * We use view capabilities as mere tokens to pass views between sessions.
 * There is no RPC interface associated with a view.
 */
struct Nitpicker::View { GENODE_RPC_INTERFACE(); };


class View : public Same_buffer_list_elem,
             public Session_view_list_elem,
             public View_stack_elem,
             public View_parent_elem,
             public Genode::Weak_object<View>,
             public Genode::Rpc_object<Nitpicker::View>
{
	public:

		enum { TITLE_LEN = 32 };   /* max.characters of a title */

		enum Transparent { NOT_TRANSPARENT = 0, TRANSPARENT = 1 };
		enum Background  { NOT_BACKGROUND  = 0, BACKGROUND  = 1 };

	private:

		Transparent const _transparent;   /* background is partly visible */
		Background        _background;    /* view is a background view    */

		View      *_parent;         /* parent view                          */
		Rect       _geometry;       /* position and size relative to parent */
		Rect       _label_rect;     /* position and size of label           */
		Point      _buffer_off;     /* offset to the visible buffer area    */
		Session   &_session;        /* session that created the view        */
		char       _title[TITLE_LEN];
		Dirty_rect _dirty_rect;

		Genode::List<View_parent_elem> _children;

		/**
		 * Assign new parent
		 *
		 * Normally, the parent of a view is defined at the construction time
		 * of the view. However, if the domain origin changes at runtime, we
		 * need to dynamically re-assign the pointer origin as the parent.
		 */
		void _assign_parent(View *parent)
		{
			if (_parent == parent)
				return;

			if (_parent)
				_parent->remove_child(*this);

			_parent = parent;

			if (_parent)
				_parent->add_child(*this);
		}

	public:

		View(Session &session, Transparent transparent,
		     Background bg, View *parent)
		:
			_transparent(transparent), _background(bg),
			_parent(parent), _session(session)
		{ title(""); }

		virtual ~View()
		{
			/* invalidate weak pointers to this object */
			lock_for_destruction();

			/* break link to our parent */
			if (_parent)
				_parent->remove_child(*this);

			/* break links to our children */
			while (View_parent_elem *e = _children.first()) {
				static_cast<View *>(e)->dissolve_from_parent();
				_children.remove(e);
			}
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

		bool has_parent(View const &parent) const { return &parent == _parent; }

		void apply_origin_policy(View &pointer_origin)
		{
			if (session().origin_pointer() && !has_parent(pointer_origin))
				_assign_parent(&pointer_origin);

			if (!session().origin_pointer() && has_parent(pointer_origin))
				_assign_parent(0);
		}

		Rect geometry() const { return _geometry; }

		void geometry(Rect geometry) { _geometry = geometry; }

		void add_child(View const &child) { _children.insert(&child); }

		void remove_child(View const &child) { _children.remove(&child); }

		template <typename FN>
		void for_each_child(FN const &fn) {
			for (View_parent_elem *e = _children.first(); e; e = e->next())
				fn(*static_cast<View *>(e));
		}

		template <typename FN>
		void for_each_const_child(FN const &fn) const {
			for (View_parent_elem const *e = _children.first(); e; e = e->next())
				fn(*static_cast<View const *>(e));
		}

		/**
		 * Return thickness of frame that surrounds the view
		 */
		virtual int frame_size(Mode const &mode) const
		{
			if (!_session.label_visible()) return 0;

			return mode.focused(_session) ? 5 : 3;
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
		 * \param bg  true if view is background
		 */
		void background(bool bg) {
			_background = bg ? BACKGROUND : NOT_BACKGROUND; }

		/**
		 * Accessors
		 */
		Session &session() const { return _session; }

		bool belongs_to(Session const &session) const { return &session == &_session; }
		bool same_session_as(View const &other) const { return &_session == &other._session; }

		bool  top_level()   const { return _parent == 0; }
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
			if (_session.content_client() && session().uses_alpha())
				return session().input_mask_at(p - view_rect.p1() - _buffer_off);

			return true;
		}

		/**
		 * Mark part of view as dirty
		 *
		 * \param rect  dirty rectangle in absolute coordinates
		 */
		void mark_as_dirty(Rect rect) { _dirty_rect.mark_as_dirty(rect); }

		/**
		 * Return dirty-rectangle information
		 */
		Dirty_rect dirty_rect() const { return _dirty_rect; }

		/**
		 * Reset dirty rectangle
		 */
		void mark_as_clean() { _dirty_rect = Dirty_rect(); }
};

#endif /* _VIEW_H_ */
