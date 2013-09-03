/*
 * \brief   Objects that are findable through unique IDs
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__OBJECT_H_
#define _KERNEL__OBJECT_H_

/* Genode includes */
#include <util/avl_tree.h>
#include <base/printf.h>

/* core includes */
#include <assert.h>

/* base-hw includes */
#include <singleton.h>

namespace Kernel
{
	template <typename T> class Avl_tree : public Genode::Avl_tree<T> { };
	template <typename T> class Avl_node : public Genode::Avl_node<T> { };

	/**
	 * Map unique sortable IDs to objects
	 *
	 * \param T  object type that inherits from Object_pool<T>::Item
	 */
	template <typename T>
	class Object_pool;

	/**
	 * Manage allocation of a static set of IDs
	 *
	 * \param SIZE  amount of allocatable IDs
	 */
	template <unsigned SIZE>
	class Id_allocator;

	/**
	 * Make all objects of a deriving class findable through unique IDs
	 */
	template <typename T, unsigned MAX_INSTANCES>
	class Object;
}

template <typename T>
class Kernel::Object_pool
{
	public:

		/**
		 * Enable a deriving class T to be inserted into an Object_pool<T>
		 */
		class Item;

		/**
		 * Insert 'object' into pool
		 */
		void insert(T * const object) { _tree.insert(object); }

		/**
		 * Remove 'object' from pool
		 */
		void remove(T * const object) { _tree.remove(object); }

		/**
		 * Return object with ID 'id', or 0 if such an object doesn't exist
		 */
		T * object(unsigned const id) const
		{
			Item * const root = _tree.first();
			if (!root) { return 0; }
			return static_cast<T *>(root->find(id));
		}

	private:

		Avl_tree<Item> _tree;
};

template <typename T>
class Kernel::Object_pool<T>::Item : public Avl_node<Item>
{
	protected:

		unsigned _id;

	public:

		/**
		 * Constructor
		 */
		Item(unsigned const id) : _id(id) { }

		/**
		 * Find entry with 'object_id' within this AVL subtree
		 */
		Item * find(unsigned const object_id)
		{
			if (object_id == id()) { return this; }
			Item * const subtree = Avl_node<Item>::child(object_id > id());
			if (!subtree) { return 0; }
			return subtree->find(object_id);
		}

		/**
		 * ID of this object
		 */
		unsigned id() const { return _id; }


		/************************
		 * 'Avl_node' interface *
		 ************************/

		bool higher(Item * i) const { return i->id() > id(); }
};

template <unsigned SIZE>
class Kernel::Id_allocator
{
	private:

		enum {
			MIN = 1,
			MAX = MIN + SIZE - 1
		};

		bool     _free[MAX + 1];
		unsigned _free_id;

		/**
		 * Return wether 'id' is a valid ID
		 */
		bool _valid_id(unsigned const id) const
		{
			return id >= MIN && id <= MAX;
		}

	public:

		/**
		 * Constructor
		 */
		Id_allocator() : _free_id(MIN)
		{
			/* free all IDs */
			for (unsigned i = MIN; i <= MAX; i++) { _free[i] = 1; }
		}

		/**
		 * Allocate a free ID
		 *
		 * \return  ID that has been allocated by the call
		 */
		unsigned alloc()
		{
			/* FIXME: let userland donate RAM to avoid out of mem */
			if (!_valid_id(_free_id)) {
				PERR("failed to allocate ID");
				while (1) { }
			}
			/* allocate _free_id */
			_free[_free_id] = 0;
			unsigned const id = _free_id;

			/* update _free_id */
			_free_id++;
			for (; _free_id <= MAX && !_free[_free_id]; _free_id++) { }
			return id;
		}

		/**
		 * Free ID 'id'
		 */
		void free(unsigned const id)
		{
			assert(_valid_id(id));
			_free[id] = 1;
			if (id < _free_id) { _free_id = id; }
		}
};

template <typename T, unsigned MAX_INSTANCES>
class Kernel::Object : public Object_pool<T>::Item
{
	private :

		class Id_allocator : public Kernel::Id_allocator<MAX_INSTANCES> { };

		/**
		 * Unique-ID allocator for objects of T
		 */
		static Id_allocator * _id_allocator()
		{
			return unsynchronized_singleton<Id_allocator>();
		}

	public:

		typedef Object_pool<T> Pool;

		/**
		 * Map of unique IDs to objects of T
		 */
		static Pool * pool() { return unsynchronized_singleton<Pool>(); }

	protected:

		/**
		 * Constructor
		 */
		Object() : Pool::Item(_id_allocator()->alloc())
		{
			pool()->insert(static_cast<T *>(this));
		}

		/**
		 * Destructor
		 */
		~Object()
		{
			pool()->remove(static_cast<T *>(this));
			_id_allocator()->free(Pool::Item::id());
		}
};

#endif /* _KERNEL__OBJECT_H_ */
