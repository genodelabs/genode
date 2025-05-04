/*
 * \brief  Utilities for object life-time management
 * \author Norman Feske
 * \date   2013-03-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__WEAK_PTR_H_
#define _INCLUDE__BASE__WEAK_PTR_H_

#include <base/blockade.h>
#include <base/mutex.h>
#include <base/log.h>
#include <base/exception.h>
#include <util/list.h>
#include <util/attempt.h>

namespace Genode {
	class Weak_object_base;
	class Weak_ptr_base;
	class Locked_ptr_base;

	template <typename T> struct Weak_object;
	template <typename T> struct Weak_ptr;
	template <typename T> struct Locked_ptr;
}


/**
 * Type-agnostic base class of a weak pointer
 *
 * This class implements the mechanics of the the 'Weak_ptr' class template.
 * It is not used directly.
 */
class Genode::Weak_ptr_base : public Genode::List<Weak_ptr_base>::Element
{
	private:

		friend class Weak_object_base;
		friend class Locked_ptr_base;

		Mutex mutable _mutex { };

		Weak_object_base *_obj { nullptr };

		/*
		 * This blocakde is used to synchronize destruction of a weak pointer
		 * and its corresponding weak object that happen simultaneously
		 */
		Blockade mutable _destruct { };

		inline void _adopt(Weak_object_base *obj);
		inline void _disassociate();

		Weak_ptr_base(Weak_ptr_base const &);

	protected:

		explicit inline Weak_ptr_base(Weak_object_base *obj);

	public:

		/**
		 * Default constructor, produces invalid pointer
		 */
		Weak_ptr_base() { }

		inline ~Weak_ptr_base();

		/**
		 * Assignment operator
		 */
		inline Weak_ptr_base &operator = (Weak_ptr_base const &other);

		/**
		 * Test for equality
		 */
		inline bool operator == (Weak_ptr_base const &other) const;

		/**
		 * Return pointer to object if it exists, or 0 if object vanished
		 *
		 * \noapi
		 */
		Weak_object_base *obj() const { return _obj; }

		/**
		 * Inspection hook for unit test
		 *
		 * \noapi
		 */
		void debug_info() const;
};


/**
 * Type-agnostic base class of a weak object
 */
class Genode::Weak_object_base
{
	private:

		friend class Weak_ptr_base;
		friend class Locked_ptr_base;

		/**
		 * List of weak pointers currently pointing to the object
		 */
		Mutex               _list_mutex { };
		List<Weak_ptr_base> _list       { };

		/**
		 * Buffers dequeued weak pointer that get invalidated currently
		 */
		Weak_ptr_base *_ptr_in_destruction = nullptr;

		/**
		 * Mutex to synchronize access to object
		 */
		Mutex _mutex { };

	protected:

		/**
		 * To be called from 'Weak_object<T>' only
		 *
		 * \noapi
		 */
		template <typename T>
		Weak_ptr<T> _weak_ptr();

	public:

		~Weak_object_base()
		{
			if (_list.first())
				error("Weak object ", this, " not destructed properly "
				     "there are still dangling pointers to it");
		}

		enum class Disassociate_error { IN_DESTRUCTION };
		using      Disassociate_result = Attempt<Ok, Disassociate_error>;

		Disassociate_result disassociate(Weak_ptr_base *ptr)
		{
			if (ptr) {
				Mutex::Guard guard(_list_mutex);

				/*
				 * If the weak pointer that tries to disassociate is currently
				 * removed to invalidate it by the weak object's destructor,
				 * signal that fact to the pointer, so it can free it's mutex,
				 * and block until invalidation is finished.
				 */
				if (_ptr_in_destruction == ptr)
					return Disassociate_error::IN_DESTRUCTION;

				_list.remove(ptr);
			}
			return Ok();
		}

		/**
		 * Mark object as safe to be destructed
		 *
		 * This method must be called by the destructor of a weak object to
		 * defer the destruction until no 'Locked_ptr' is held to the object.
		 */
		void lock_for_destruction()
		{
			/*
			 * Loop through the list of weak pointers and invalidate them
			 */
			while (true) {

				/*
				 * To prevent dead-locks we always have to hold
				 * the order of mutex access, therefore we first
				 * dequeue one weak pointer and free the list mutex again
				 */
				{
					Mutex::Guard guard(_list_mutex);
					_ptr_in_destruction = _list.first();

					/* if the list is empty we're done */
					if (!_ptr_in_destruction) break;
					_list.remove(_ptr_in_destruction);
				}

				{
					Mutex::Guard guard(_ptr_in_destruction->_mutex);
					_ptr_in_destruction->_obj = nullptr;

					/*
					 * unblock a weak pointer that tried to disassociate
					 * in the meantime
					 */
					_ptr_in_destruction->_destruct.wakeup();
				}
			}

			/*
			 * synchronize with locked pointers that already acquired
			 * the mutex before the corresponding weak pointer got invalidated
			 */
			_mutex.acquire();
		}

		/**
		 * Inspection hook for unit test
		 *
		 * \noapi
		 */
		void debug_info() const;
};


class Genode::Locked_ptr_base
{
	private:

		/*
		 * Noncopyable
		 */
		Locked_ptr_base(Locked_ptr_base const &);
		Locked_ptr_base &operator = (Locked_ptr_base const &);

	protected:

		Weak_object_base *curr = nullptr;

		/**
		 * Constructor
		 *
		 * \noapi
		 */
		inline Locked_ptr_base(Weak_ptr_base &weak_ptr);

		/**
		 * Destructor
		 *
		 * \noapi
		 */
		inline ~Locked_ptr_base();
};


/**
 * Weak pointer to a given type
 *
 * A weak pointer can be obtained from a weak object (an object that inherits
 * the 'Weak_object' class template) and safely survives the lifetime of the
 * associated weak object. If the weak object disappears, all
 * weak pointers referring to the object are automatically invalidated.
 * To avoid race conditions between the destruction and use of a weak object,
 * a weak pointer cannot be de-reference directly. To access the object, a
 * weak pointer must be turned into a locked pointer ('Locked_ptr').
 */
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
	inline Weak_ptr &operator = (Weak_ptr<T> const &other)
	{
		*static_cast<Weak_ptr_base *>(this) = other;
		return *this;
	}
};


/**
 * Weak object
 *
 * This class template must be inherited in order to equip an object with
 * the weak-pointer mechanism.
 *
 * \param T  type of the derived class
 */
template <typename T>
struct Genode::Weak_object : Genode::Weak_object_base
{
	/**
	 * Obtain a weak pointer referring to the weak object
	 */
	Weak_ptr<T> weak_ptr() { return _weak_ptr<T>(); }

	/**
	 * Const version of 'weak_ptr'
	 *
	 * This function is useful in cases where the returned weak pointer is
	 * merely used for comparison operations.
	 */
	Weak_ptr<T const> const weak_ptr_const() const
	{
		/*
		 * We strip off the constness of 'this' to reuse the internal non-const
		 * code of the weak object. The executed operations are known to not
		 * alter the state of the weak object.
		 */
		return const_cast<Weak_object *>(this)->_weak_ptr<T const>();
	}
};


/**
 * Locked pointer
 *
 * A locked pointer is constructed from a weak pointer. After construction,
 * its validity can (and should) be checked by calling the 'valid'
 * method. If the locked pointer is valid, the pointed-to object is known to
 * be locked until the locked pointer is destroyed. During this time, the
 * locked pointer can safely be de-referenced.
 *
 * The typical pattern of using a locked pointer is to declare it as a
 * local variable. Once the execution leaves the scope of the variable, the
 * locked pointer is destructed, which unlocks the pointed-to weak object.
 * It effectively serves as a lock guard.
 */
template <typename T>
struct Genode::Locked_ptr : Genode::Locked_ptr_base
{
	Locked_ptr(Weak_ptr<T> &weak_ptr) : Locked_ptr_base(weak_ptr) { }

	T *operator -> () { return static_cast<T *>(curr); }

	T &operator * () { return *static_cast<T *>(curr); }

	/**
	 * Returns true if the locked pointer is valid
	 *
	 * Only if valid, the locked pointer can be de-referenced. Otherwise,
	 * the attempt will result in a null-pointer access.
	 */
	bool valid() const { return curr != nullptr; }
};


/********************
 ** Implementation **
 ********************/

void Genode::Weak_ptr_base::_adopt(Genode::Weak_object_base *obj)
{
	_obj = obj;

	if (_obj) {
		Mutex::Guard guard(_obj->_list_mutex);
		_obj->_list.insert(this);
	}
}


void Genode::Weak_ptr_base::_disassociate()
{
	/* defer destruction of object */
	Mutex::Guard guard(_mutex);

	if (!_obj)
		return;

	if (_obj->disassociate(this) == Weak_object_base::Disassociate_error::IN_DESTRUCTION)
		_destruct.block();
}


Genode::Weak_ptr_base::Weak_ptr_base(Genode::Weak_object_base *obj)
{
	_adopt(obj);
}


Genode::Weak_ptr_base &
Genode::Weak_ptr_base::operator = (Weak_ptr_base const &other)
{
	/* self assignment */
	if (&other == this)
		return *this;

	_disassociate();
	{
		Mutex::Guard guard(other._mutex);
		_adopt(other._obj);
	}
	return *this;
}


bool Genode::Weak_ptr_base::operator == (Weak_ptr_base const &other) const
{
	if (&other == this)
		return true;

	Mutex::Guard guard_this(_mutex), guard_other(other._mutex);

	return (_obj == other._obj);
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


Genode::Locked_ptr_base::Locked_ptr_base(Weak_ptr_base &weak_ptr)
: curr(nullptr)
{
	Mutex::Guard guard(weak_ptr._mutex);

	if (!weak_ptr.obj()) return;

	curr = weak_ptr.obj();
	curr->_mutex.acquire();
}


Genode::Locked_ptr_base::~Locked_ptr_base()
{
	if (curr)
		curr->_mutex.release();
}

#endif /* _INCLUDE__BASE__WEAK_PTR_H_ */
