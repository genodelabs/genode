/*
 * \brief  Object pool - map ids to objects
 * \author Norman Feske
 * \date   2006-06-26
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__OBJECT_POOL_H_
#define _INCLUDE__BASE__OBJECT_POOL_H_

#include <util/avl_tree.h>
#include <base/capability.h>
#include <base/lock.h>

namespace Genode {

	/**
	 * Map object ids to local objects
	 *
	 * \param OBJ_TYPE  object type (must be inherited from Object_pool::Entry)
	 *
	 * The local names of a capabilities are used to differentiate multiple server
	 * objects managed by one and the same object pool.
	 */
	template <typename OBJ_TYPE>
	class Object_pool
	{
		public:

			class Entry : public Avl_node<Entry>
			{
				private:

					Untyped_capability _cap;

					inline long _obj_id() { return _cap.local_name(); }

					friend class Object_pool;
					friend class Avl_tree<Entry>;

				public:

					enum { OBJ_ID_INVALID = 0 };

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
					Entry *find_by_obj_id(long obj_id)
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

			/**
			 * Lookup object
			 */
			OBJ_TYPE *obj_by_id(long obj_id)
			{
				Lock::Guard lock_guard(_lock);
				Entry *obj = _tree.first();
				return (OBJ_TYPE *)(obj ? obj->find_by_obj_id(obj_id) : 0);
			}

			OBJ_TYPE *obj_by_cap(Untyped_capability cap)
			{
				return obj_by_id(cap.local_name());
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
	};
}

#endif /* _INCLUDE__BASE__OBJECT_POOL_H_ */
