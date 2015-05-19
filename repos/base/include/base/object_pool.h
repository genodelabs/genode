/*
 * \brief  Object pool - map ids to objects
 * \author Norman Feske
 * \author Alexander Boettcher
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
 * Map object ids to local objects
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

		class Guard
		{
			private:

				OBJ_TYPE * _object;

			public:
				operator OBJ_TYPE*()    const { return _object; }
				OBJ_TYPE * operator->() const { return _object; }
				OBJ_TYPE * object()     const { return _object; }

				template <class X>
				explicit Guard(X * object) {
					_object = dynamic_cast<OBJ_TYPE *>(object); }

				~Guard()
				{
					if (!_object) return;

					_object->release();
				}
		};

		class Entry : public Avl_node<Entry>
		{
			private:

				Untyped_capability _cap;
				short int          _ref;
				bool               _dead;

				Lock               _entry_lock;

				inline unsigned long _obj_id() { return _cap.local_name(); }

				friend class Object_pool;
				friend class Avl_tree<Entry>;

				/*
				 * Support methods for atomic lookup and lock functionality of
				 * class Object_pool.
				 */

				void lock()   { _entry_lock.lock(); };
				void unlock() { _entry_lock.unlock(); };

				void add_ref() { _ref += 1; }
				void del_ref() { _ref -= 1; }

				bool is_dead(bool set_dead = false) {
					return (set_dead ? (_dead = true) : _dead); }
				bool is_ref_zero() { return _ref <= 0; }

			public:

				/**
				 * Constructors
				 */
				Entry() : _ref(0), _dead(false) { }
				Entry(Untyped_capability cap) : _cap(cap), _ref(0), _dead(false) { }

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

				/**
				 * Function used - ideally - solely by the Guard.
				 */
				void release() { del_ref(); unlock(); }
				void acquire() { lock(); add_ref();   }
		};

	private:

		Avl_tree<Entry> _tree;
		Lock            _lock;

	public:

		void insert(OBJ_TYPE *obj)
		{
			Lock::Guard lock_guard(_lock);
			_tree.insert(obj);
		}

		void remove_locked(OBJ_TYPE *obj)
		{
			obj->is_dead(true);
			obj->del_ref();

			while (true) {
				obj->unlock();
				{
					Lock::Guard lock_guard(_lock);
					if (obj->is_ref_zero()) {
						_tree.remove(obj);
						return;
					}
				}
				obj->lock();
			}
		}

		/**
		 * Lookup object
		 */
		OBJ_TYPE *lookup_and_lock(addr_t obj_id)
		{
			OBJ_TYPE * obj_typed;
			{
				Lock::Guard lock_guard(_lock);
				Entry *obj = _tree.first();
				if (!obj) return 0;

				obj_typed = (OBJ_TYPE *)obj->find_by_obj_id(obj_id);
				if (!obj_typed || obj_typed->is_dead())
					return 0;

				obj_typed->add_ref();
			}

			obj_typed->lock();
			return obj_typed;
		}

		OBJ_TYPE *lookup_and_lock(Untyped_capability cap)
		{
			return lookup_and_lock(cap.local_name());
		}

		/**
		 * Return first element of tree
		 *
		 * This function is used for removing tree elements step by step.
		 */
		OBJ_TYPE *first()
		{
			Lock::Guard lock_guard(_lock);
			return (OBJ_TYPE *)_tree.first();
		}

		/**
		 * Return first element of tree locked
		 *
		 * This function is used for removing tree elements step by step.
		 */
		OBJ_TYPE *first_locked()
		{
			Lock::Guard lock_guard(_lock);
			OBJ_TYPE * const obj_typed = (OBJ_TYPE *)_tree.first();
			if (!obj_typed) { return 0; }
			obj_typed->lock();
			return obj_typed;
		}
};

#endif /* _INCLUDE__BASE__OBJECT_POOL_H_ */
