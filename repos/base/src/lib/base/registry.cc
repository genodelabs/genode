/*
 * \brief  Thread-safe object registry
 * \author Norman Feske
 * \date   2016-11-06
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/registry.h>
#include <base/thread.h>

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
		Mutex::Guard guard(_mutex);
		if (_notify_ptr && _registry._curr == this) {

			/*
			 * The destructor is called from the functor of a
			 * 'Registry::for_each' loop with the element temporarily dequeued.
			 * We flag the element to be not re-inserted into the list.
			 */
			_notify_ptr->keep = Notify::Keep::DISCARD;

			/* done if and only if running in the context of same thread */
			if (Thread::myself() == _notify_ptr->thread)
				return;

			/*
			 * We synchronize on the _mutex of the _registry, by invoking
			 * the _remove method below. This ensures that the object leaves
			 * the destructor not before the registry lost the pointer to this
			 * object. The actual removal attempt will be ignored by the list
			 * implementation, since the current object was removed already.
			 */
		}
	}
	_registry._remove(*this);
}


void Registry_base::_insert(Element &element)
{
	Mutex::Guard guard(_mutex);

	_elements.insert(&element);
}


void Registry_base::_remove(Element &element)
{
	Mutex::Guard guard(_mutex);

	_elements.remove(&element);
}


Registry_base::Element *Registry_base::_processed(Notify &notify,
                                                  List<Element> &processed,
                                                  Element &e, Element *at)
{
	_curr = nullptr;

	/* if 'e' was dropped from the list, keep the current re-insert position */
	if (notify.keep == Notify::Keep::DISCARD)
		return at;

	/* make sure that the critical section of '~Element' is completed */
	Mutex::Guard guard(e._mutex);

	/* here we know that 'e' still exists */
	e._notify_ptr = nullptr;

	/*
	 * If '~Element' was preempted between the condition check and the
	 * assignment of keep = DISCARD, the above check would miss the DISCARD
	 * flag. Now, with the acquired mutex, we know that the 'keep' value is
	 * up to date.
	 */
	if (notify.keep == Notify::Keep::DISCARD)
		return at;

	/* insert 'e' into the list of processed elements */
	processed.insert(&e, at);

	/* advance insert position to 'e' */
	return &e;
}


void Registry_base::_for_each(Untyped_functor &functor)
{
	Mutex::Guard guard(_mutex);

	/* insert position in list of processed elements */
	Element *at = nullptr;

	List<Element> processed;

	while (Element *e = _elements.first()) {

		Notify notify(Notify::Keep::KEEP, Thread::myself());
		{
			/* tell the element where to report its status */
			Mutex::Guard guard(e->_mutex);
			_curr = e;
			e->_notify_ptr = &notify;
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
		catch (...) {

			/* handle current element */
			at = _processed(notify, processed, *e, at);

			/* handle the remaining elements without invoking the functor */
			while (Element *e = _elements.first()) {
				_elements.remove(e);
				at = _processed(notify, processed, *e, at);
			};
			_elements = processed;

			/* propagate exception to caller */
			throw;
		}

		at = _processed(notify, processed, *e, at);
	}

	/* use list of processed elements as '_elements' list */
	_elements = processed;
}
