/*
 * \brief  Thread-safe object registry
 * \author Norman Feske
 * \date   2016-11-06
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/registry.h>

using namespace Genode;


Registry_base::Element::Element(Registry_base &registry, void *obj)
:
	_registry(registry), _obj(obj)
{
	_registry._insert(*this);
}


Registry_base::Element::~Element()
{
	{
		Lock::Guard guard(_lock);
		if (_keep_ptr && _registry._curr == this) {

			/*
			 * The destructor is called from the functor of a
			 * 'Registry::for_each' loop with the element temporarily dequeued.
			 * We flag the element to be not re-inserted into the list.
			 */
			*_keep_ptr = DISCARD;
			return;
		}
	}
	_registry._remove(*this);
}


void Registry_base::_insert(Element &element)
{
	Lock::Guard lock_guard(_lock);

	_elements.insert(&element);
}


void Registry_base::_remove(Element &element)
{
	Lock::Guard lock_guard(_lock);

	_elements.remove(&element);
}


Registry_base::Element *Registry_base::_processed(Keep const keep,
                                                  List<Element> &processed,
                                                  Element &e, Element *at)
{
	_curr = nullptr;

	/* if 'e' was dropped from the list, keep the current re-insert position */
	if (keep == DISCARD)
		return at;

	/* make sure that the critical section of '~Element' is completed */
	Lock::Guard guard(e._lock);

	/*
	 * If '~Element' was preempted between the condition check and the
	 * assignment of keep = DISCARD, the above check would miss the DISCARD
	 * flag. Now, with the acquired lock, we know that the 'keep' value is
	 * up to date.
	 */
	if (keep == DISCARD)
		return at;

	/* here we know that 'e' still exists */
	e._keep_ptr = nullptr;

	/* insert 'e' into the list of processed elements */
	processed.insert(&e, at);

	/* advance insert position to 'e' */
	return &e;
}


void Registry_base::_for_each(Untyped_functor &functor)
{
	Lock::Guard lock_guard(_lock);

	/* insert position in list of processed elements */
	Element *at = nullptr;

	List<Element> processed;

	while (Element *e = _elements.first()) {

		Keep keep = KEEP;
		{
			/* tell the element where to report its status */
			Lock::Guard guard(e->_lock);
			_curr = e;
			e->_keep_ptr = &keep;
		}

		/*
		 * Remove the element from the list. Depending on the behavior of 'fn'
		 * (whether it destroys 'e' or not, we will insert it into the
		 * 'processed' list afterwards.
		 */
		_elements.remove(e);

		/* the element may disappear during the call of 'fn' */
		try { functor.call(e->_obj); }

		/* propagate exceptions while keeping registry consistent */
		catch (...) { at = _processed(keep, processed, *e, at); throw; }

		at = _processed(keep, processed, *e, at);
	}

	/* use list of processed elements as '_elements' list */
	_elements = processed;
}
