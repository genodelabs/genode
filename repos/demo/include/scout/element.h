/*
 * \brief  Scout GUI element
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SCOUT__ELEMENT_H_
#define _INCLUDE__SCOUT__ELEMENT_H_

#include <scout/event.h>
#include <scout/canvas.h>

namespace Scout {
	class Element;
	class Parent_element;
}


class Scout::Element
{
	protected:

		Point _position;              /* relative position managed by parent */
		Area  _size;                  /* size managed by parent              */
		Area  _min_size;              /* min size managed by element         */
		Parent_element *_parent;      /* parent in element hierarchy         */
		Event_handler  *_evh;         /* event handler object                */
		struct {
			int mfocus      : 1;      /* element has mouse focus             */
			int selected    : 1;      /* element has selected state          */
			int takes_focus : 1;      /* element highlights mouse focus      */
			int chapter     : 1;      /* display element as single page      */
			int findable    : 1;      /* regard element in find function     */
			int bottom      : 1;      /* place element to the bottom         */
		} _flags;

	public:

		Element mutable *next = { 0 };        /* managed by parent */

		/**
		 * Constructor
		 */
		Element() : _parent(0), _evh(0)
		{
			_flags.mfocus      = _flags.selected = 0;
			_flags.takes_focus = 0;
			_flags.chapter     = 0;
			_flags.findable    = 1;
			_flags.bottom      = 0;
		}

		/**
		 * Destructor
		 */
		virtual ~Element();

		/**
		 * Accessor functionse
		 */
		Point position() const { return _position; }
		Area  size()     const { return _size; }
		Area  min_size() const { return _min_size; }
		bool  bottom()   const { return _flags.bottom; }

		void findable(int flag) { _flags.findable = flag; }

		/**
		 * Set geometry of the element
		 *
		 * This function should only be called by the immediate parent
		 * element.
		 */
		virtual void geometry(Rect rect)
		{
			_position = rect.p1();
			_size     = rect.area();
		}

		/**
		 * Set/reset the mouse focus
		 */
		virtual void mfocus(int flag)
		{
			if ((_flags.mfocus == flag) || !_flags.takes_focus) return;
			_flags.mfocus = flag;
			refresh();
		}

		/**
		 * Define/request parent of an element
		 */
		void parent(Parent_element *parent) { _parent = parent; }
		Parent_element *parent() { return _parent; }

		bool has_parent(Parent_element const *parent) const { return parent == _parent; }

		/**
		 * Define event handler object
		 */
		void event_handler(Event_handler *evh) { _evh = evh; }

		/**
		 * Check if element is completely clipped and draw it otherwise
		 */
		void try_draw(Canvas_base &canvas, Point abs_position)
		{
			Rect const abs_rect = Rect(abs_position + _position, _size);

			/* check if element is completely outside the clipping area */
			if (!Rect::intersect(canvas.clip(), abs_rect).valid())
				return;

			/* call actual drawing function */
			draw(canvas, abs_position);
		}

		/**
		 * Format element and all child elements to specified width
		 */
		virtual void format_fixed_width(int w) { }

		/**
		 * Format element and all child elements to specified width and height
		 */
		virtual void format_fixed_size(Area size) { }

		/**
		 * Draw function
		 *
		 * This function must not be called directly.
		 * Instead, the function try_draw should be called.
		 */
		virtual void draw(Canvas_base &, Point) { }

		/**
		 * Find top-most element at specified position
		 *
		 * The default implementation can be used for elements without
		 * children. It just the element position and size against the
		 * specified position.
		 */
		virtual Element *find(Point);

		/**
		 * Find the back-most element at specified y position
		 *
		 * This function is used to query a document element at
		 * the current scroll position of the window. This way,
		 * we can adjust the y position to the right value
		 * when we browse the history.
		 */
		virtual Element *find_by_y(int y);

		/**
		 * Request absolute position of an element
		 */
		Point abs_position() const;

		/**
		 * Update area of an element on screen
		 *
		 * We propagate the redraw request through the element hierarchy to
		 * the parent. The root parent should overwrite this function with
		 * a function that performs the actual redraw.
		 */
		virtual void redraw_area(int x, int y, int w, int h);

		/**
		 * Trigger the refresh of an element on screen
		 */
		void refresh() { redraw_area(0, 0, _size.w(), _size.h()); }

		/**
		 * Handle user input or timer event
		 */
		void handle_event(Event const &ev) { if (_evh) _evh->handle_event(ev); }

		/**
		 * Request the chapter in which the element lives
		 */
		Element *chapter();

		/**
		 * Fill image cache for element
		 */
		virtual void fill_cache(Canvas_base &) { }

		/**
		 * Flush image cache for element
		 */
		virtual void flush_cache(Canvas_base &) { }

		/**
		 * Execute function for each sibling including the element itself
		 *
		 * The functor FUNC takes a reference to the element as argument.
		 *
		 * This function template is implemented in 'scout/parent_element.h'.
		 */
		template <typename FUNC>
		inline void for_each_sibling(FUNC func);
};

#endif /* _INCLUDE__SCOUT__ELEMENT_H_ */
