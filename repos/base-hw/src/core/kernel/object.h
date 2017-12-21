/*
 * \brief   Kernel object identities and references
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__OBJECT_H_
#define _CORE__KERNEL__OBJECT_H_

/* Genode includes */
#include <util/avl_tree.h>
#include <util/bit_allocator.h>
#include <util/list.h>

/* core includes */
#include <kernel/interface.h>
#include <kernel/kernel.h>

namespace Kernel
{
	class Pd; /* forward declaration */

	/**
	 * Base class of all Kernel objects
	 */
	class Object;

	/**
	 * An object identity helps to distinguish different capability owners
	 * that reference a Kernel object
	 */
	class Object_identity;

	/**
	 * An object identity reference is the in-kernel representation
	 * of a PD local capability. It references an object identity and is
	 * associated with a protection domain.
	 */
	class Object_identity_reference;

	/**
	 * A tree of object identity references to retrieve the capabilities
	 * of one PD fastly.
	 */
	class Object_identity_reference_tree;

	using Object_identity_reference_list
		= Genode::List<Object_identity_reference>;

	using Object_identity_list
		= Genode::List<Kernel::Object_identity>;

	/**
	 * This class represents kernel object's identities including the
	 * corresponding object identity reference for core
	 */
	template <typename T> class Core_object_identity;

	/**
	 * This class represents a kernel object, it's identity, and the
	 * corresponding object identity reference for core
	 */
	template <typename T> class Core_object;
}


struct Kernel::Object : private Object_identity_list
{
	using Object_identity_list::remove;
	using Object_identity_list::insert;

	virtual ~Object();
};


class Kernel::Object_identity
: public Object_identity_list::Element,
  public Kernel::Object_identity_reference_list
{
	private:

		/*
		 * Noncopyable
		 */
		Object_identity(Object_identity const &);
		Object_identity &operator = (Object_identity const &);

		Object * _object = nullptr;

	public:

		Object_identity(Object & object);
		~Object_identity();

		template <typename KOBJECT>
		KOBJECT * object() { return dynamic_cast<KOBJECT*>(_object); }

		void invalidate();
};


class Kernel::Object_identity_reference
: public Genode::Avl_node<Kernel::Object_identity_reference>,
  public Genode::List<Kernel::Object_identity_reference>::Element
{
	private:

		/*
		 * Noncopyable
		 */
		Object_identity_reference(Object_identity_reference const &);
		Object_identity_reference &operator = (Object_identity_reference const &);

		capid_t          _capid;
		Object_identity *_identity;
		Pd              &_pd;
		unsigned short   _in_utcbs;

	public:

		Object_identity_reference(Object_identity *oi, Pd &pd);
		~Object_identity_reference();

		/***************
		 ** Accessors **
		 ***************/

		template <typename KOBJECT>
		KOBJECT * object() {
			return _identity ? _identity->object<KOBJECT>() : nullptr; }

		Object_identity_reference * factory(void * dst, Pd &pd);

		Pd &    pd()     { return _pd;    }
		capid_t capid()  { return _capid; }

		void add_to_utcb()      { _in_utcbs++; }
		void remove_from_utcb() { _in_utcbs--; }
		bool in_utcb()          { return _in_utcbs > 0; }

		void invalidate();


		/************************
		 ** Avl_node interface **
		 ************************/

		bool higher(Object_identity_reference * oir) const {
			return oir->_capid > _capid; }


		/**********************
		 ** Lookup functions **
		 **********************/

		Object_identity_reference * find(Pd * pd);
		Object_identity_reference * find(capid_t capid);
};


class Kernel::Object_identity_reference_tree
: public Genode::Avl_tree<Kernel::Object_identity_reference>
{
	public:

		Object_identity_reference * find(capid_t id);

		template <typename KOBJECT>
		KOBJECT * find(capid_t id)
		{
			Object_identity_reference * oir = find(id);
			return (oir) ? oir->object<KOBJECT>() : nullptr;
		}
};


template <typename T>
class Kernel::Core_object_identity : public Object_identity,
                                     public Object_identity_reference
{
	public:

		Core_object_identity(T & object)
		: Object_identity(object),
		  Object_identity_reference(this, *core_pd()) { }

		capid_t core_capid() { return capid(); }
};


template <typename T>
class Kernel::Core_object : public T, Kernel::Core_object_identity<T>
{
	public:

		template <typename... ARGS>
		Core_object(ARGS &&... args)
		: T(args...), Core_object_identity<T>(*static_cast<T*>(this)) { }

		using Kernel::Core_object_identity<T>::core_capid;
		using Kernel::Core_object_identity<T>::capid;
};

#endif /* _CORE__KERNEL__OBJECT_H_ */
