/*
 * \brief  Utilities for object life-time management
 * \author Norman Feske
 * \date   2013-03-09
 *
 * This header provides utilities for avoiding dangling pointers. Such a
 * situation happens when an object disappears while pointers to the object
 * are still in use. One way to solve this problem is to explicitly notify the
 * holders of those pointers about the disappearance of the object. But this
 * would require the object to keep references to those pointer holder, which,
 * in turn, might disappear as well. Consequently, this approach tends to
 * become a complex solution, which is prone to deadlocks or race conditions
 * when multiple threads are involved.
 *
 * The utilities provided herein implement a more elegant pattern called
 * "weak pointers" to deal with such situations. An object that might
 * disappear at any time is represented by the 'Weak_object' class
 * template. It keeps track of a list of so-called weak pointers pointing
 * to the object. A weak pointer, in turn, holds privately the pointer to the
 * object alongside a validity flag. It cannot be used to dereference the
 * object. For accessing the actual object, a locked pointer must be created
 * from a weak pointer. If this creation succeeds, the object is guaranteed to
 * be locked (not destructed) until the locked pointer gets destroyed. If the
 * object no longer exists, the locked pointer will be invalid. This condition
 * can (and should) be detected via the 'Locked_ptr::is_valid()' function prior
 * dereferencing the pointer.
 *
 * In the event a weak object gets destructed, all weak pointers that point
 * to the object are automatically invalidated. So a subsequent conversion into
 * a locked pointer will yield an invalid pointer, which can be detected (in
 * contrast to a dangling pointer).
 *
 * To use this mechanism, the destruction of a weak object must be
 * deferred until no locked pointer points to the object anymore. This is
 * done by calling the function 'Weak_object::lock_for_destruction()'
 * at the beginning of the destructor of the to-be-destructed object.
 * When this function returns, all weak pointers to the object will have been
 * invalidated. So it is save to destruct and free the object.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__WEAK_PTR_H_
#define _INCLUDE__BASE__WEAK_PTR_H_

#include <base/lock.h>
#include <util/list.h>

namespace Genode {
	class Weak_object_base;
	class Weak_ptr_base;
	class Locked_ptr_base;

	template <typename T> struct Weak_object;
	template <typename T> struct Weak_ptr;
	template <typename T> struct Locked_ptr;
}


class Genode::Weak_ptr_base : public Genode::List<Weak_ptr_base>::Element
{
	private:

		friend class Weak_object_base;
		friend class Locked_ptr_base;

		Lock mutable      _lock;
		Weak_object_base *_obj;
		bool              _valid; /* true if '_obj' points to an
		                             existing object */

		inline void _adopt(Weak_object_base *obj);
		inline void _disassociate();

	protected:

		Weak_object_base *obj() const { return _valid ? _obj: 0; }

		explicit inline Weak_ptr_base(Weak_object_base *obj);

	public:

		/**
		 * Default constructor, produces invalid pointer
		 */
		inline Weak_ptr_base();

		inline ~Weak_ptr_base();

		/**
		 * Assignment operator
		 */
		inline void operator = (Weak_ptr_base const &other);

		/**
		 * Test for equality
		 */
		inline bool operator == (Weak_ptr_base const &other) const;

		/**
		 * Inspection hook for unit test
		 */
		void debug_info() const;
};


class Genode::Weak_object_base
{
	private:

		friend class Weak_ptr_base;
		friend class Locked_ptr_base;

		/**
		 * List of weak pointers currently pointing to the object
		 */
		Lock                _list_lock;
		List<Weak_ptr_base> _list;

		/**
		 * Lock used to defer the destruction of an object derived from
		 * 'Weak_object_base'
		 */
		Lock _destruct_lock;

	protected:

		inline ~Weak_object_base();

		/**
		 * To be called from 'Weak_object<T>' only
		 */
		template <typename T>
		Weak_ptr<T> _weak_ptr();

	public:

		/**
		 * Function to be called by the destructor of a weak object to
		 * defer the destruction until no 'Locked_ptr' is held to the object.
		 */
		void lock_for_destruction() { _destruct_lock.lock(); }

		/**
		 * Inspection hook for unit test
		 */
		void debug_info() const;
};


class Genode::Locked_ptr_base
{
	protected:

		Weak_object_base *curr;

		inline Locked_ptr_base(Weak_ptr_base &weak_ptr);
		inline ~Locked_ptr_base();
};


template <typename T>
struct Genode::Weak_ptr : Genode::Weak_ptr_base
{
	/**
	 * Default constructor creates invalid pointer
	 */
	Weak_ptr() { }

	/**
	 * Copy constructor
	 */
	Weak_ptr(Weak_ptr<T> const &other) : Weak_ptr_base(other.obj()) { }

	/**
	 * Assignment operator
	 */
	inline void operator = (Weak_ptr<T> const &other)
	{
		*static_cast<Weak_ptr_base *>(this) = other;
	}
};


template <typename T>
struct Genode::Weak_object : Genode::Weak_object_base
{
	Weak_ptr<T> weak_ptr() { return _weak_ptr<T>(); }
};


template <typename T>
struct Genode::Locked_ptr : Genode::Locked_ptr_base
{
	Locked_ptr(Weak_ptr<T> &weak_ptr) : Locked_ptr_base(weak_ptr) { }

	T *operator -> () { return static_cast<T *>(curr); }

	T &operator * () { return *static_cast<T *>(curr); }

	bool is_valid() const { return curr != 0; }
};


/********************
 ** Implementation **
 ********************/

void Genode::Weak_ptr_base::_adopt(Genode::Weak_object_base *obj)
{
	if (!obj)
		return;

	_obj = obj;
	_valid = true;

	Lock::Guard guard(_obj->_list_lock);
	_obj->_list.insert(this);
}


void Genode::Weak_ptr_base::_disassociate()
{
	/* defer destruction of object */
	{
		Lock::Guard guard(_lock);

		if (!_valid)
			return;

		_obj->_destruct_lock.lock();
	}

	/*
	 * Disassociate reference from object
	 *
	 * Because we hold the '_destruct_lock', we are safe to do
	 * the list operation. However, after we have released the
	 * 'Weak_ptr_base::_lock', the object may have invalidated
	 * the reference. So we must check for validity again.
	 */
	{
		Lock::Guard guard(_obj->_list_lock);
		if (_valid)
			_obj->_list.remove(this);
	}

	/* release object */
	_obj->_destruct_lock.unlock();
}


Genode::Weak_ptr_base::Weak_ptr_base(Genode::Weak_object_base *obj)
{
	_adopt(obj);
}


Genode::Weak_ptr_base::Weak_ptr_base() : _obj(0), _valid(false) { }


void Genode::Weak_ptr_base::operator = (Weak_ptr_base const &other)
{
	/* self assignment */
	if (&other == this)
		return;

	Weak_object_base *obj = other.obj();
	_disassociate();
	_adopt(obj);
}


bool Genode::Weak_ptr_base::operator == (Weak_ptr_base const &other) const
{
	if (&other == this)
		return true;

	Lock::Guard guard_this(_lock), guard_other(other._lock);

	return (!_valid && !other._valid)
	    || (_valid && other._valid && _obj == other._obj);
}


Genode::Weak_ptr_base::~Weak_ptr_base()
{
	_disassociate();
}


template <typename T>
Genode::Weak_ptr<T> Genode::Weak_object_base::_weak_ptr()
{
	Weak_ptr_base result(this);
	return *static_cast<Weak_ptr<T> *>(&result);
}


Genode::Weak_object_base::~Weak_object_base()
{
	{
		Lock::Guard guard(_list_lock);

		Weak_ptr_base *curr = 0;
		while ((curr = _list.first())) {

			Lock::Guard guard(curr->_lock);
			curr->_valid = false;
			_list.remove(curr);
		}
	}
}


Genode::Locked_ptr_base::Locked_ptr_base(Weak_ptr_base &weak_ptr)
: curr(0)
{
	Lock::Guard guard(weak_ptr._lock);

	if (!weak_ptr._valid)
		return;

	curr = weak_ptr._obj;
	curr->_destruct_lock.lock();
}


Genode::Locked_ptr_base::~Locked_ptr_base()
{
	if (curr)
		curr->_destruct_lock.unlock();
}

#endif /* _INCLUDE__BASE__WEAK_PTR_H_ */
