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
#include <base/lock.h>
#include <base/log.h>
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

		class Out_of_ids     : Exception { };
		class Conflicting_id : Exception { };

		class Element : public Avl_node<Element>, Noncopyable
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

				template <typename ARG, typename FUNC>
				void _for_each(FUNC const &fn) const
				{
					if (Avl_node<Element>::child(Avl_node_base::LEFT))
						Avl_node<Element>::child(Avl_node_base::LEFT)->_for_each<ARG>(fn);

					fn(static_cast<ARG &>(_obj));

					if (Avl_node<Element>::child(Avl_node_base::RIGHT))
						Avl_node<Element>::child(Avl_node_base::RIGHT)->_for_each<ARG>(fn);
				}

			public:

				/**
				 * Constructor
				 *
				 * \throw Out_of_ids  ID space is exhausted
				 */
				Element(T &obj, Id_space &id_space)
				:
					_obj(obj), _id_space(id_space)
				{
					Lock::Guard guard(_id_space._lock);
					_id = id_space._unused_id(*this);
					_id_space._elements.insert(this);
				}

				/**
				 * Constructor
				 *
				 * \throw Conflicting_id  'id' is already present in ID space
				 */
				Element(T &obj, Id_space &id_space, Id_space::Id id)
				:
					_obj(obj), _id_space(id_space), _id(id)
				{
					Lock::Guard guard(_id_space._lock);
					_id_space._check_conflict(*this, id);
					_id_space._elements.insert(this);
				}

				~Element()
				{
					Lock::Guard guard(_id_space._lock);
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
 
		Lock mutable      _lock;       /* protect '_elements' and '_cnt' */
		Avl_tree<Element> _elements;
		unsigned long     _cnt = 0;

		/**
		 * Return ID that does not exist within the ID space
		 *
		 * \return ID assigned to the element within the ID space
		 * \throw  Out_of_ids
		 */
		Id _unused_id(Element &e)
		{
			unsigned long _attempts = 0;
			for (; _attempts < ~0UL; _attempts++, _cnt++) {

				Id const id { _cnt };

				/* another attempt if is already in use */
				if (_elements.first() && _elements.first()->_lookup(id))
					continue;

				return id;
			}
			throw Out_of_ids();
		}

		/**
		 * Check if ID is already in use
		 *
		 * \throw  Conflicting_id
		 */
		void _check_conflict(Element &e, Id id)
		{
			if (_elements.first() && _elements.first()->_lookup(id))
				throw Conflicting_id();
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
		template <typename ARG, typename FUNC>
		void for_each(FUNC const &fn) const
		{
			Lock::Guard guard(_lock);

			if (_elements.first())
				_elements.first()->template _for_each<ARG>(fn);
		}

		/**
		 * Apply functor 'fn' to object with given ID
		 *
		 * See 'for_each' for a description of the 'ARG' argument.
		 *
		 * \throw Unknown_id
		 */
		template <typename ARG, typename FUNC>
		auto apply(Id id, FUNC const &fn)
		-> typename Trait::Functor<decltype(&FUNC::operator())>::Return_type
		{
			T *obj = nullptr;
			{
				Lock::Guard guard(_lock);

				if (!_elements.first())
					throw Unknown_id();

				if (Element *e = _elements.first()->_lookup(id))
					obj = &e->_obj;
			}
			if (obj)
				return fn(static_cast<ARG &>(*obj));
			else
				throw Unknown_id();
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
		template <typename ARG, typename FUNC>
		bool apply_any(FUNC const &fn)
		{
			T *obj = nullptr;
			{
				Lock::Guard guard(_lock);

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
