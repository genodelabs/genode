/*
 * \brief  ID name space
 * \author Norman Feske
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ID_SPACE_H_
#define _INCLUDE__BASE__ID_SPACE_H_

#include <util/noncopyable.h>
#include <util/meta.h>
#include <base/mutex.h>
#include <base/log.h>
#include <base/exception.h>
#include <util/avl_tree.h>

namespace Genode { template <typename T> class Id_space; }


template <typename T>
class Genode::Id_space : public Noncopyable
{
	public:

		struct Id
		{
			unsigned long value;

			bool operator == (Id const &other) const { return value == other.value; }

			void print(Output &out) const { Genode::print(out, value); }
		};

		class Element : public Avl_node<Element>
		{
			private:

				T        &_obj;
				Id_space &_id_space;
				Id        _id { 0 };

				friend class Id_space;

				/**
				 * Search the tree for the element with the given ID
				 */
				Element *_lookup(Id id)
				{
					if (id.value == _id.value) return this;

					Element *e = Avl_node<Element>::child(id.value > _id.value);

					return e ? e->_lookup(id) : 0;
				}

				template <typename ARG>
				void _for_each(auto const &fn) const
				{
					if (Avl_node<Element>::child(Avl_node_base::LEFT))
						Avl_node<Element>::child(Avl_node_base::LEFT)->template _for_each<ARG>(fn);

					fn(static_cast<ARG &>(_obj));

					if (Avl_node<Element>::child(Avl_node_base::RIGHT))
						Avl_node<Element>::child(Avl_node_base::RIGHT)->template _for_each<ARG>(fn);
				}

			public:

				/**
				 * Constructor
				 */
				Element(T &obj, Id_space &id_space)
				:
					_obj(obj), _id_space(id_space)
				{
					Mutex::Guard guard(_id_space._mutex);
					_id = id_space._unused_id();
					_id_space._elements.insert(this);
				}

				/**
				 * Constructor
				 */
				Element(T &obj, Id_space &id_space, Id_space::Id id)
				:
					_obj(obj), _id_space(id_space), _id(id)
				{
					Mutex::Guard guard(_id_space._mutex);
					_id_space._check_conflict(id);
					_id_space._elements.insert(this);
				}

				~Element()
				{
					Mutex::Guard guard(_id_space._mutex);
					_id_space._elements.remove(this);
				}

				/**
				 * Avl_node interface
				 */
				bool higher(Element *other) { return other->_id.value > _id.value; }

				Id id() const { return _id; }

				void print(Output &out) const { Genode::print(out, _id); }
		};

	private:
 
		Mutex mutable     _mutex    { };   /* protect '_elements' and '_cnt' */
		Avl_tree<Element> _elements { };
		unsigned long     _cnt = 0;

		/**
		 * Return ID that does not exist within the ID space
		 *
		 * \return ID assigned to the element within the ID space
		 */
		Id _unused_id()
		{
			unsigned long _attempts = 0;
			for (; _attempts < ~0UL; _attempts++, _cnt++) {

				Id const id { _cnt };

				/* another attempt if is already in use */
				if (_elements.first() && _elements.first()->_lookup(id))
					continue;

				return id;
			}
			/*
			 * The number of IDs exhausts the number of unsigned long values.
			 * In this hypothetical case, accept ID ambiguities.
			 */
			return { ~0UL };
		}

		void _check_conflict(Id id)
		{
			/*
			 * The ambiguity is not fatal to the integrity of the ID space
			 * but it hints strongly at a bug at the user of the ID space.
			 * Hence, print a diagnostic error but do not escalate.
			 */
			if (_elements.first() && _elements.first()->_lookup(id))
				error("ID space misused with ambiguous IDs");
		}

	public:

		class Unknown_id : Exception { };

		/**
		 * Apply functor 'fn' to each ID present in the ID space
		 *
		 * \param ARG  argument type passed to 'fn', must be convertible
		 *             from 'T' via a 'static_cast'
		 *
		 * This function is called with the ID space locked. Hence, it is not
		 * possible to modify the ID space from within 'fn'.
		 */
		template <typename ARG>
		void for_each(auto const &fn) const
		{
			Mutex::Guard guard(_mutex);

			if (_elements.first())
				_elements.first()->template _for_each<ARG>(fn);
		}

		/**
		 * Apply functor 'fn' to object with given ID, or call 'missing_fn'
		 *
		 * See 'for_each' for a description of the 'ARG' argument.
		 * If the ID is not known, 'missing_fn' is called instead of 'fn'.
		 * Both 'fn' and 'missing_fn' must have the same return type.
		 */
		template <typename ARG, typename FN>
		auto apply(Id id, FN const &fn, auto const &missing_fn)
		-> typename Trait::Functor<decltype(&FN::operator())>::Return_type
		{
			T *obj_ptr = nullptr;
			{
				Mutex::Guard guard(_mutex);

				if (_elements.first())
					if (Element *e = _elements.first()->_lookup(id))
						obj_ptr = &e->_obj;
			}
			if (obj_ptr)
				return fn(static_cast<ARG &>(*obj_ptr));
			else
				return missing_fn();
		}

		/**
		 * Apply functor 'fn' to object with given ID
		 *
		 * See 'for_each' for a description of the 'ARG' argument.
		 *
		 * \throw Unknown_id
		 */
		template <typename ARG, typename FN>
		auto apply(Id id, FN const &fn)
		-> typename Trait::Functor<decltype(&FN::operator())>::Return_type
		{
			using Result = typename Trait::Functor<decltype(&FN::operator())>::Return_type;
			return apply<ARG>(id, fn, [&] () -> Result { throw Unknown_id(); });
		}

		/**
		 * Apply functor 'fn' to an arbitrary ID present in the ID space
		 *
		 * See 'for_each' for a description of the 'ARG' argument.
		 *
		 * The functor is called with a reference to the managed object as
		 * argument. This method is designated for the destruction of ID
		 * spaces. It allows the caller to remove all IDs by subseqently
		 * calling this function and destructing the object in 'fn'.
		 *
		 * \return  true if 'fn' was applied, or
		 *          false if the ID space is empty.
		 */
		template <typename ARG>
		bool apply_any(auto const &fn)
		{
			T *obj = nullptr;
			{
				Mutex::Guard guard(_mutex);

				if (_elements.first())
					obj = &_elements.first()->_obj;
				else
					return false;
			}
			fn(static_cast<ARG &>(*obj));
			return true;
		}

		~Id_space()
		{
			if (_elements.first())
				error("ID space not empty at destruction time");
		}
};

#endif /* _INCLUDE__BASE__ID_SPACE_H_ */
