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

#ifndef _INCLUDE__BASE__REGISTRY_H_
#define _INCLUDE__BASE__REGISTRY_H_

#include <util/list.h>
#include <base/lock.h>

namespace Genode {

	class Registry_base;
	template <typename T> struct Registry;
	template <typename T> class Registered;
}


class Genode::Registry_base
{
	private:

		enum Keep { KEEP, DISCARD };

	protected:

		class Element : public List<Element>::Element
		{
			private:

				friend class Registry_base;

				Registry_base &_registry;

				/**
				 * Protect '_reinsert_ptr'
				 */
				Lock _lock;

				/*
				 * Assigned by 'Registry::_for_each'
				 */
				Keep *_keep_ptr = nullptr;

			protected:

				void * const _obj;

			public:

				Element(Registry_base &, void *);

				~Element();
		};

	protected:

		Lock mutable  _lock; /* protect '_elements' */
		List<Element> _elements;

	private:

		/**
		 * Element currently processed by '_for_each'
		 */
		Element const *_curr = nullptr;

		void _insert(Element &);
		void _remove(Element &);

		Element *_processed(Keep, List<Element> &, Element &, Element *);

	protected:

		struct Untyped_functor { virtual void call(void *obj_ptr) = 0; };

		void _for_each(Untyped_functor &);
};


template <typename T>
struct Genode::Registry : private Registry_base
{
	struct Element : Registry_base::Element
	{
		friend class Registry;  /* allow 'for_each' to access '_obj' */

		Element(Registry &registry, T &obj)
		: Registry_base::Element(registry, &obj) { }
	};

	template <typename FUNC>
	void for_each(FUNC const &fn)
	{
		struct Typed_functor : Registry_base::Untyped_functor
		{
			FUNC const &_fn;
			Typed_functor(FUNC const &fn) : _fn(fn) { }

			void call(void *obj_ptr) override
			{
				T &obj = *reinterpret_cast<T *>(obj_ptr);
				_fn(obj);
			}
		} untyped_functor(fn);

		Registry_base::_for_each(untyped_functor);
	}

	template <typename FUNC>
	void for_each(FUNC const &fn) const
	{
		Lock::Guard lock_guard(_lock);

		Registry_base::Element const *e = _elements.first(), *next = nullptr;
		for ( ; e; e = next) {
			next = e->next();
			Element const &typed_element = static_cast<Element const &>(*e);
			T const &obj = *reinterpret_cast<T const *>(typed_element._obj);
			fn(obj);
		}
	}
};


/**
 * Convenience helper to equip a type 'T' with a 'Registry::Element'
 *
 * Using this helper, an arbitrary type can be turned into a registry element
 * type. E.g., in order to keep 'Child_service' objects in a registry, a new
 * registry-compatible type can be created via 'Registered<Child_service>'.
 * Objects of this type can be kept in a 'Registry<Registered<Child_service>
 * >'. The constructor of such "registered" objects expect the registry as the
 * first argument. The other arguments are forwarded to the constructor of the
 * enclosed type.
 */
template <typename T>
class Genode::Registered : public T
{
	private:

		typename Registry<Registered<T> >::Element _element;

	public:

		template <typename... ARGS>
		Registered(Registry<Registered<T> > &registry, ARGS &&... args)
		: T(args...), _element(registry, *this) { }
};

#endif /* _INCLUDE__BASE__REGISTRY_H_ */
