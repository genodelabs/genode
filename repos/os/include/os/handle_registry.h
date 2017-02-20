/*
 * \brief  Utility for managing object handles
 * \author Norman Feske
 * \date   2014-06-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__HANDLE_REGISTRY_H_
#define _INCLUDE__OS__HANDLE_REGISTRY_H_

#include <util/avl_tree.h>
#include <base/weak_ptr.h>
#include <base/tslab.h>

namespace Genode {

	template <typename T>                    class Handle;
	template <typename HANDLE, typename OBJ> class Handle_registry;
}


/**
 * Typed handle
 */
template <typename T>
class Genode::Handle
{
	private:

		static constexpr unsigned _invalid() { return ~0; }

		unsigned _value = _invalid();

	public:

		typedef T Type;

		Handle() { }

		Handle(unsigned value) : _value(value) { }

		unsigned value() const { return _value; }

		bool valid() const { return _value != _invalid(); }

		bool operator == (Handle const &other) const { return other.value() == _value; }
};


/**
 * Registry of handles, which refer to objects
 *
 * \param HANDLE  handle type
 * \param OBJ     type of context associated with a handle
 *
 * The constructor of the 'HANDLE' type must take an unsigned value as argument
 * and have a 'value()' method that returns the same value.
 *
 * The 'OBJ' type must be inherited from 'Weak_object'.
 */
template <typename HANDLE, typename OBJ>
class Genode::Handle_registry
{
	private:

		/**
		 * Meta data that keeps the association of a handle with an object
		 */
		struct Element : public Avl_node<Element>,
		                 public HANDLE,
		                 public Weak_ptr<OBJ>
		{
			Element(Weak_ptr<OBJ> weak_ptr, unsigned id)
			:
				HANDLE(id), Weak_ptr<OBJ>(weak_ptr)
			{ }

			/**
			 * Avl_node interface
			 */
			bool higher(Element *e) {
				return e->HANDLE::value() > HANDLE::value(); }

			/**
			 * Traverse AVL tree to find handle by id
			 */
			Element *find_by_handle(HANDLE const &handle)
			{
				if (handle.value() == HANDLE::value()) return this;

				Element * const e =
					Avl_node<Element>::child(handle.value() > HANDLE::value());

				return e ? e->find_by_handle(handle) : 0;
			}
		};

		Tslab<Element, 4000> _slab;

		unsigned _cnt = 0;

		Avl_tree<Element> _elements;

		Element &_lookup(HANDLE handle) const
		{
			Element *result = nullptr;

			if (_elements.first())
				result = _elements.first()->find_by_handle(handle);

			if (result)
				return *result;

			throw Lookup_failed();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  allocator used for allocating the meta data for the
		 *               handles
		 */
		Handle_registry(Allocator &alloc) : _slab(&alloc) { }

		~Handle_registry()
		{
			while (_elements.first())
				free(*_elements.first());
		}

		/**
		 * Exception types
		 */
		class Lookup_failed { };
		class Out_of_memory { };

		/**
		 * Allocate handle for specified object
		 *
		 * \param handle  designated handle to assign to the object. By
		 *                default, a new handle gets allocated.
		 *
		 * \throw Out_of_memory
		 */
		HANDLE &alloc(OBJ &obj, HANDLE handle = HANDLE())
		{
			/* disassociate original object from supplied handle */
			try {
				if (handle.valid())
					free(handle);
			} catch (Lookup_failed) { }

			try {
				Element *e = new (_slab)
					Element(obj.weak_ptr(),
					        handle.valid() ? handle.value() : ++_cnt);

				_elements.insert(e);
				return *e;
			}
			catch (Allocator::Out_of_memory) {
				throw Out_of_memory(); }
		}

		/**
		 * Release handle
		 *
		 * \throw Lookup_failed
		 */
		void free(HANDLE handle)
		{
			Element &e = _lookup(handle);
			_elements.remove(&e);
			destroy(_slab, &e);
		}

		/**
		 * Lookup pointer to OBJ by a given handle
		 *
		 * \throw Lookup_failed
		 */
		Weak_ptr<OBJ> &lookup(HANDLE handle) const { return _lookup(handle); }

		/**
		 * Return true if 'obj' is registered under the specified handle
		 *
		 * \throw Lookup_failed
		 */
		bool has_handle(OBJ const &obj, HANDLE handle) const
		{
			return obj.weak_ptr_const() == _lookup(handle);
		}
};

#endif /* _INCLUDE__OS__HANDLE_REGISTRY_H_ */
