/*
 * \brief  Scout GUI parent element
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__PARENT_ELEMENT_H_
#define _INCLUDE__SCOUT__PARENT_ELEMENT_H_

#include <scout/element.h>

namespace Scout {
	class Element;
	class Parent_element;
}

class Scout::Parent_element : public Element
{
	private:

		/*
		 * Noncopyable
		 */
		Parent_element(Parent_element const &);
		Parent_element &operator = (Parent_element const &);

	protected:

		Element *_first = nullptr;
		Element *_last  = nullptr;

		/**
		 * Format child element by a given width an horizontal offset
		 */
		int _format_children(int x, int w);

	public:

		/**
		 * Constructor
		 */
		Parent_element() { _first = _last = 0; }

		/**
		 * Adopt a child element
		 */
		void append(Element *e);

		/**
		 * Release child element from parent element
		 */
		void remove(Element const *e);

		/**
		 * Dispose references to the specified element
		 *
		 * The element is not necessarily an immediate child but some element
		 * of the element-subtree.  This function gets propagated to the root
		 * parent (e.g., user state manager), which can reset the mouse focus
		 * of the focused element vanishes.
		 */
		virtual void forget(Element const *e);

		/**
		 * Element interface
		 */
		void     draw(Canvas_base &, Point);
		Element *find(Point);
		Element *find_by_y(int);
		void     fill_cache(Canvas_base &);
		void     flush_cache(Canvas_base &);
		void     geometry(Rect);

		/**
		 * Execute function on each child
		 *
		 * The functor FUNC takes a reference to the element as argument.
		 */
		template <typename FUNC>
		void for_each_child(FUNC func)
		{
			for (Element *e = _first; e; e = e->next)
				func(*e);
		}
};


template <typename FUNC>
void Scout::Element::for_each_sibling(FUNC func)
{
	if (parent())
		parent()->for_each_child(func);
}


#endif /* _INCLUDE__SCOUT__PARENT_ELEMENT_H_ */
