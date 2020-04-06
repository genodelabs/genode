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

#include <util/interface.h>
#include <util/list.h>
#include <base/mutex.h>

namespace Genode {

	class Registry_base;
	template <typename T> struct Registry;
	template <typename T> class Registered;
	template <typename T> class Registered_no_delete;
}


class Genode::Registry_base
{
	private:

		struct Notify
		{
			enum Keep { KEEP, DISCARD } keep;
			void * const thread;

			Notify(Keep k, void *t) : keep(k), thread(t) { }
		};

	protected:

		class Element : public List<Element>::Element
		{
			private:

				friend class Registry_base;

				Registry_base &_registry;

				Mutex _mutex { };

				/*
				 * Assigned by 'Registry::_for_each'
				 */
				Notify *_notify_ptr = nullptr;

				/*
				 * Noncopyable
				 */
				Element(Element const &);
				Element &operator = (Element const &);

			protected:

				void * const _obj;

			public:

				Element(Registry_base &, void *);

				~Element();
		};

	protected:

		Mutex mutable _mutex    { }; /* protect '_elements' */
		List<Element> _elements { };

	private:

		/**
		 * Element currently processed by '_for_each'
		 */
		Element const *_curr = nullptr;

		void _insert(Element &);
		void _remove(Element &);

		Element *_processed(Notify &, List<Element> &, Element &, Element *);

	protected:

		struct Untyped_functor : Interface { virtual void call(void *obj_ptr) = 0; };

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
		Mutex::Guard guard(_mutex);

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
 * Objects of this type can be kept in a 'Registry<Registered<Child_service> >'.
 * The constructor of such "registered" objects expect the registry as the
 * first argument. The other arguments are forwarded to the constructor of the
 * enclosed type.
 */
template <typename T>
class Genode::Registered : public T
{
	private:

		typename Registry<Registered<T> >::Element _element;

	public:

		/**
		 * Compile-time check
		 *
		 * \noapi
		 */
		static_assert(__has_virtual_destructor(T), "registered object must have virtual destructor");

		template <typename... ARGS>
		Registered(Registry<Registered<T> > &registry, ARGS &&... args)
		: T(args...), _element(registry, *this) { }
};


/**
 * Variant of Registered that does not require a vtable in the base class
 *
 * The generic 'Registered' convenience class requires the base class to
 * provide a vtable resp. a virtual destructor for safe deletion of a base
 * class pointer. By using 'Registered_no_delete', this requirement can be
 * lifted.
 */
template <typename T>
class Genode::Registered_no_delete : public T
{
	private:

		typename Registry<Registered_no_delete<T> >::Element _element;

	public:

		template <typename... ARGS>
		Registered_no_delete(Registry<Registered_no_delete<T> > &registry, ARGS &&... args)
		: T(args...), _element(registry, *this) { }
};

#endif /* _INCLUDE__BASE__REGISTRY_H_ */
