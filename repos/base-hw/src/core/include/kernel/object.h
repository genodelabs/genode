/*
 * \brief   Objects that are findable through unique IDs
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__OBJECT_H_
#define _KERNEL__OBJECT_H_

/* Genode includes */
#include <util/avl_tree.h>
#include <util/bit_allocator.h>

/* core includes */
#include <assert.h>
#include <kernel/configuration.h>

namespace Kernel
{
	/**
	 * Map unique sortable IDs to objects
	 *
	 * \param T  object type that inherits from Object_pool<T>::Item
	 */
	template <typename T>
	class Object_pool;

	/**
	 * Manage allocation of a static set of IDs
	 */
	using Id_allocator = Genode::Bit_allocator<MAX_KERNEL_OBJECTS>;
	Id_allocator & id_alloc();

	/**
	 * Make all objects of a deriving class findable through unique IDs
	 *
	 * \param T object type
	 * \param POOL accessor function of object pool
	 */
	template <typename T, Kernel::Object_pool<T> * (* POOL)()>
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

		Genode::Avl_tree<Item> _tree;
};

template <typename T>
class Kernel::Object_pool<T>::Item : public Genode::Avl_node<Item>
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
			Item * const subtree =
				Genode::Avl_node<Item>::child(object_id > id());
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


template <typename T, Kernel::Object_pool<T> * (* POOL)()>
class Kernel::Object : public Object_pool<T>::Item
{
	public:

		using Pool = Object_pool<T>;

		/**
		 * Map of unique IDs to objects of T
		 */
		static Pool * pool() { return POOL(); }

	protected:

		Object() : Pool::Item(id_alloc().alloc()) {
			POOL()->insert(static_cast<T *>(this)); }

		~Object()
		{
			POOL()->remove(static_cast<T *>(this));
			id_alloc().free(Pool::Item::id());
		}
};

#endif /* _KERNEL__OBJECT_H_ */
