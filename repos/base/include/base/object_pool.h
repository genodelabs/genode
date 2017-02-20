/*
 * \brief  Object pool - map capabilities to objects
 * \author Norman Feske
 * \author Alexander Boettcher
 * \author Stafen Kalkowski
 * \date   2006-06-26
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__OBJECT_POOL_H_
#define _INCLUDE__BASE__OBJECT_POOL_H_

#include <util/avl_tree.h>
#include <util/noncopyable.h>
#include <base/capability.h>
#include <base/weak_ptr.h>

namespace Genode { template <typename> class Object_pool; }


/**
 * Map capabilities to local objects
 *
 * \param OBJ_TYPE  object type (must be inherited from Object_pool::Entry)
 *
 * The local names of a capabilities are used to differentiate multiple server
 * objects managed by one and the same object pool.
 */
template <typename OBJ_TYPE>
class Genode::Object_pool
{
	public:

		class Entry : public Avl_node<Entry>
		{
			private:

				friend class Object_pool;
				friend class Avl_tree<Entry>;

				struct Entry_lock : Weak_object<Entry_lock>, Noncopyable
				{
					Entry &obj;

					Entry_lock(Entry &e) : obj(e) { }
					~Entry_lock() {
						Weak_object<Entry_lock>::lock_for_destruction(); }
				};

				Untyped_capability _cap;
				Entry_lock         _lock { *this };

				inline unsigned long _obj_id() { return _cap.local_name(); }

			public:

				Entry() { }
				Entry(Untyped_capability cap) : _cap(cap) { }

				virtual ~Entry() { }

				/**
				 * Avl_node interface
				 */
				bool higher(Entry *e) { return e->_obj_id() > _obj_id(); }
				void recompute() { }  /* for gcc-3.4 compatibility */

				/**
				 * Support for object pool
				 */
				Entry *find_by_obj_id(unsigned long obj_id)
				{
					if (obj_id == _obj_id()) return this;

					Entry *obj = this->child(obj_id > _obj_id());

					return obj ? obj->find_by_obj_id(obj_id) : 0;
				}

				/**
				 * Assign capability to object pool entry
				 */
				void cap(Untyped_capability c) { _cap = c; }

				Untyped_capability const cap() const { return _cap; }
		};

	private:

		Avl_tree<Entry> _tree;
		Lock            _lock;

	protected:

		bool empty()
		{
			Lock::Guard lock_guard(_lock);
			return _tree.first() == nullptr;
		}

	public:

		void insert(OBJ_TYPE *obj)
		{
			Lock::Guard lock_guard(_lock);
			_tree.insert(obj);
		}

		void remove(OBJ_TYPE *obj)
		{
			Lock::Guard lock_guard(_lock);
			_tree.remove(obj);
		}

		template <typename FUNC>
		auto apply(unsigned long capid, FUNC func)
		-> typename Trait::Functor<decltype(&FUNC::operator())>::Return_type
		{
			using Functor        = Trait::Functor<decltype(&FUNC::operator())>;
			using Object_pointer = typename Functor::template Argument<0>::Type;
			using Weak_ptr       = Weak_ptr<typename Entry::Entry_lock>;
			using Locked_ptr     = Locked_ptr<typename Entry::Entry_lock>;

			Weak_ptr ptr;

			{
				Lock::Guard lock_guard(_lock);

				Entry * entry = _tree.first() ?
					_tree.first()->find_by_obj_id(capid) : nullptr;

				if (entry) ptr = entry->_lock.weak_ptr();
			}

			{
				Locked_ptr lock_ptr(ptr);
				Object_pointer op = lock_ptr.valid()
					? dynamic_cast<Object_pointer>(&lock_ptr->obj) : nullptr;
				return func(op);
			}
		}

		template <typename FUNC>
		auto apply(Untyped_capability cap, FUNC func)
		-> typename Trait::Functor<decltype(&FUNC::operator())>::Return_type
		{
			return apply(cap.local_name(), func);
		}

		template <typename FUNC>
		void remove_all(FUNC func)
		{
			using Weak_ptr   = Weak_ptr<typename Entry::Entry_lock>;
			using Locked_ptr = Locked_ptr<typename Entry::Entry_lock>;

			for (;;) {
				OBJ_TYPE * obj;

				{
					Lock::Guard lock_guard(_lock);

					if (!((obj = (OBJ_TYPE*) _tree.first()))) return;

					Weak_ptr ptr = obj->_lock.weak_ptr();
					{
						Locked_ptr lock_ptr(ptr);
						if (!lock_ptr.valid()) return;

						_tree.remove(obj);
					}
				}

				func(obj);
			}
		}
};

#endif /* _INCLUDE__BASE__OBJECT_POOL_H_ */
