/*
 * \brief  Utility for implementing animated objects
 * \author Norman Feske
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__ANIMATOR_H_
#define _INCLUDE__GEMS__ANIMATOR_H_

/* Genode includes */
#include <util/list.h>


class Animator
{
	public:

		class Item;

	private:

		friend class Item;

		Genode::List<Genode::List_element<Item> > _items { };

	public:

		inline void animate();

		bool active() const { return _items.first() != nullptr; }
};


/**
 * Interface to be implemented by animated objects
 */
class Animator::Item
{
	private:

		Genode::List_element<Item> _element { this };

		Animator &_animator;
		bool      _animated = false;

	public:

		Item(Animator &animator) : _animator(animator) { }

		virtual ~Item() { animated(false); }

		virtual void animate() = 0;

		void animated(bool animated)
		{
			if (animated == _animated)
				return;

			if (animated)
				_animator._items.insert(&_element);
			else
				_animator._items.remove(&_element);

			_animated = animated;
		}

		bool animated() const { return _animated; }
};


inline void Animator::animate()
{
	for (Genode::List_element<Item> *item = _items.first(); item; ) {
		Genode::List_element<Item> *next = item->next();
		item->object()->animate();
		item = next;
	}
}

#endif /* _INCLUDE__GEMS__ANIMATOR_H_ */
