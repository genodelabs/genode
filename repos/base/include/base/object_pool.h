/*
 * \brief  Object pool - map capabilities to objects
 * \author Norman Feske
 * \author Alexander Boettcher
 * \author Stafen Kalkowski
 * \date   2006-06-26
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__OBJECT_POOL_H_
#define _INCLUDE__BASE__OBJECT_POOL_H_

#include <util/avl_tree.h>
#include <base/capability.h>
#include <base/lock.h>

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

				Untyped_capability _cap;
				Lock               _lock;

				inline unsigned long _obj_id() { return _cap.local_name(); }

				friend class Object_pool;
				friend class Avl_tree<Entry>;

			public:

				/**
				 * Constructors
				 */
				Entry() { }
				Entry(Untyped_capability cap) : _cap(cap) { }

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
				Lock& lock() { return _lock; }
		};

	private:

		Avl_tree<Entry> _tree;
		Lock            _lock;

		OBJ_TYPE* _obj_by_capid(unsigned long capid)
		{
			Entry *ret = _tree.first() ? _tree.first()->find_by_obj_id(capid)
			                           : nullptr;
			return static_cast<OBJ_TYPE*>(ret);
		}

		template <typename FUNC, typename RET>
		struct Apply_functor
		{
			RET operator()(OBJ_TYPE *obj, FUNC f)
			{
				using Functor = Trait::Functor<decltype(&FUNC::operator())>;
				using Object_pointer = typename Functor::template Argument<0>::Type;

				try {
					auto ret = f(dynamic_cast<Object_pointer>(obj));
					if (obj) obj->_lock.unlock();
					return ret;
				} catch(...) {
					if (obj) obj->_lock.unlock();
					throw;
				}
			}
		};

		template <typename FUNC>
		struct Apply_functor<FUNC, void>
		{
			void operator()(OBJ_TYPE *obj, FUNC f)
			{
				using Functor = Trait::Functor<decltype(&FUNC::operator())>;
				using Object_pointer = typename Functor::template Argument<0>::Type;

				try {
					f(dynamic_cast<Object_pointer>(obj));
					if (obj) obj->_lock.unlock();
				} catch(...) {
					if (obj) obj->_lock.unlock();
					throw;
				}
			}
		};

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
			using Functor = Trait::Functor<decltype(&FUNC::operator())>;

			OBJ_TYPE * obj;

			{
				Lock::Guard lock_guard(_lock);

				obj = _obj_by_capid(capid);

				if (obj) obj->_lock.lock();
			}

			Apply_functor<FUNC, typename Functor::Return_type> hf;
			return hf(obj, func);
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
			for (;;) {
				OBJ_TYPE * obj;

				{
					Lock::Guard lock_guard(_lock);

					obj = (OBJ_TYPE*) _tree.first();

					if (!obj) return;

					{
						Lock::Guard object_guard(obj->_lock);
						_tree.remove(obj);
					}
				}

				func(obj);
			}
		}
};

#endif /* _INCLUDE__BASE__OBJECT_POOL_H_ */
