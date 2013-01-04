/*
 * \brief  Utility for implementing animated objects
 * \author Norman Feske
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ANIMATOR_H_
#define _ANIMATOR_H_

class Animator
{
	public:

		class Item;

	private:

		friend class Item;

		Genode::List<Item> _items;

	public:

		inline void animate();
};


/**
 * Interface to be implemented by animated objects
 */
class Animator::Item : public Genode::List<Item>::Element
{
	private:

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
				_animator._items.insert(this);
			else
				_animator._items.remove(this);

			_animated = animated;
		}

		bool animated() const { return _animated; }
};


inline void Animator::animate()
{
	for (Item *item = _items.first(); item; item = item->next())
		item->animate();
}

#endif /* _ANIMATOR_H_ */
