/*
 * \brief  Nitpicker view interface
 * \author Norman Feske
 * \date   2006-08-08
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW_H_
#define _VIEW_H_

/* Genode includes */
#include <util/string.h>
#include <util/list.h>
#include <base/weak_ptr.h>
#include <base/rpc_server.h>
#include <base/registry.h>
#include <base/tslab.h>

/* local includes */
#include <canvas.h>
#include <view_owner.h>
#include <resizeable_texture.h>

namespace Nitpicker {

	class Buffer;
	class Focus;

	/*
	 * For each buffer, there is a list of views that belong to this buffer.
	 */
	struct Same_buffer_list_elem : List<Same_buffer_list_elem>::Element { };
	
	/*
	 * The view stack holds a list of all visible view in stacking order.
	 */
	struct View_stack_elem : List<View_stack_elem>::Element { };
	
	/*
	 * If a view has a parent, it is a list element of its parent view
	 */
	struct View_parent_elem : List<View_parent_elem>::Element { };

	/*
	 * Each session maintains a list of views owned by the session.
	 */
	struct Session_view_list_elem : List<Session_view_list_elem>::Element { };

	class View;
}


class Nitpicker::View : private Same_buffer_list_elem,
                        private Session_view_list_elem,
                        private View_stack_elem,
                        private View_parent_elem,
                        private Weak_object<View>,
                        public  Rpc_object<Gui::View>
{
	public:

		using Weak_object<View>::weak_ptr;

	private:

		friend class View_stack;
		friend class Gui_session;
		friend class Locked_ptr<View>;

		/*
		 * Noncopyable
		 */
		View(View const &);
		View &operator = (View const &);

		bool  const _transparent;     /* background is partly visible */
		bool        _background;      /* view is a background view    */

		View       *_parent;          /* parent view                          */
		Rect        _geometry   { };  /* position and size relative to parent */
		Rect        _label_rect { };  /* position and size of label */
		Point       _buffer_off { };  /* offset to the visible buffer area    */
		View_owner &_owner;
		Title       _title      { "" };

		Resizeable_texture<Pixel> const &_texture;

		List<View_parent_elem> _children { };

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

		struct Attr { bool transparent, background; };

		View(View_owner &owner, Resizeable_texture<Pixel> const &texture,
		     Attr attr, View *parent)
		:
			_transparent(attr.transparent), _background(attr.background),
			_parent(parent), _owner(owner), _texture(texture)
		{ }

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
			return Rect(abs_position(), _geometry.area);
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

		void apply_origin_policy(View &pointer_origin);

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
		virtual int frame_size(Focus const &) const;

		/**
		 * Draw view-surrounding frame on canvas
		 */
		virtual void frame(Canvas_base &canvas, Focus const &) const;

		/**
		 * Draw view on canvas
		 */
		virtual void draw(Canvas_base &, Font const &, Focus const &) const;

		/**
		 * Set view title
		 */
		void title(Font const &, Title const &);

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
		void background(bool bg) { _background = bg; }

		/**
		 * Accessors
		 */
		View_owner &owner() const { return _owner; }

		bool owned_by(View_owner const &owner) const
		{
			return &owner == &_owner;
		}

		bool same_owner_as(View const &other) const
		{
			return &_owner == &other._owner;
		}

		bool  top_level()   const { return _parent == 0; }
		bool  transparent() const;
		bool  background()  const { return _background; }
		Rect  label_rect()  const { return _label_rect; }
		bool  uses_alpha()  const;
		Point buffer_off()  const { return _buffer_off; }

		template <typename FN>
		void with_title(FN const &fn) const { fn(_title); }

		void buffer_off(Point buffer_off) { _buffer_off = buffer_off; }

		void label_pos(Point pos) { _label_rect = Rect(pos, _label_rect.area); }

		/**
		 * Return true if input at screen position 'p' refers to the view
		 */
		bool input_response_at(Point p) const;
};

#endif /* _VIEW_H_ */
