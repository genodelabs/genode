/*
 * \brief  Utility for in-place (re-)construction of objects
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__RECONSTRUCTIBLE_H_
#define _INCLUDE__UTIL__RECONSTRUCTIBLE_H_

#include <util/construct_at.h>
#include <util/misc_math.h>
#include <util/noncopyable.h>
#include <base/stdint.h>
#include <base/error.h>

namespace Genode {
	template<typename> class Reconstructible;
	template<typename> class Constructible;
}


/**
 * Place holder for an object to be repeatedly constructed and destructed
 *
 * This class template acts as a smart pointer that refers to an object
 * contained within the smart pointer itself. The contained object may be
 * repeatedly constructed and destructed while staying in the same place. This
 * is useful for replacing aggregated members during the lifetime of a compound
 * object.
 *
 * \param  T type
 */
template <typename T>
class Genode::Reconstructible : Noncopyable
{
	private:

		/**
		 * Static reservation of memory for the embedded object
		 */
		alignas(max(alignof(T), sizeof(addr_t))) char _space[sizeof(T)];

		/**
		 * True if constructed object is present
		 */
		bool _constructed = false;

		void _do_construct(auto &&... args)
		{
			construct_at<T>(_space, args...);
			_constructed = true;
		}

		T       *_ptr()             { return reinterpret_cast<T       *>(_space); }
		T const *_const_ptr() const { return reinterpret_cast<T const *>(_space); }

		void _check_constructed() const
		{
			if (!_constructed)
				raise(Unexpected_error::ACCESS_UNCONSTRUCTED_OBJ);
		}

	protected:

		/**
		 * Dummy type used as a hook for 'Constructible' to bypass the
		 * default constructor by invoking the 'Reconstructible(Lazy *)'
		 * constructor.
		 */
		struct Lazy { };

		/**
		 * Constructor that omits the initial construction of the object
		 */
		Reconstructible(Lazy *) { }

	public:

		/**
		 * Constructor
		 *
		 * The arguments are forwarded to the constructor of the embedded
		 * object.
		 */
		Reconstructible(auto &&... args) { _do_construct(args...); }

		~Reconstructible() { destruct(); }

		/**
		 * Construct new object in place
		 *
		 * If the 'Reconstructible' already hosts a constructed object, the old
		 * object will be destructed first.
		 */
		void construct(auto &&... args)
		{
			destruct();
			_do_construct(args...);
		}

		/**
		 * Destruct object
		 */
		void destruct()
		{
			if (!_constructed)
				return;

			/* invoke destructor */
			_ptr()->~T();

			_constructed = false;
		}

		/**
		 * Return true of constructed object is present
		 */
		bool constructed() const { return _constructed; }

		/**
		 * Construct or destruct object according to 'condition'
		 */
		void conditional(bool condition, auto &&... args)
		{
			if (condition && !constructed())
				construct(args...);

			if (!condition && constructed())
				destruct();
		}

		/**
		 * Access contained object
		 */
		T       *operator -> ()       { _check_constructed(); return       _ptr(); }
		T const *operator -> () const { _check_constructed(); return _const_ptr(); }

		T       &operator * ()       { _check_constructed(); return       *_ptr(); }
		T const &operator * () const { _check_constructed(); return *_const_ptr(); }

		void print(Output &out) const
		{
			if (_constructed)
				_const_ptr()->print(out);
			else
				out.out_string("<unconstructed>");
		}
};


template <typename T>
class Genode::Reconstructible<T &> : Noncopyable
{
	private:

		T *_ptr;

		T const *_const_ptr() const { return reinterpret_cast<T const *>(_ptr); }

		void _check_non_null() const
		{
			if (!_ptr)
				raise(Unexpected_error::ACCESS_UNCONSTRUCTED_OBJ);
		}

		Reconstructible operator = (Reconstructible const &);
		Reconstructible(Reconstructible const &);

	protected:

		struct Lazy { };

		Reconstructible(Lazy *) : _ptr(nullptr) { }

	public:

		Reconstructible(T &ref) : _ptr(&ref) { }

		void construct(T &ref) { _ptr = &ref; }
		void destruct()        { _ptr = nullptr; }

		bool constructed() const { return _ptr != nullptr; }

		void conditional(bool condition, T &ref)
		{
			if (condition && !constructed()) construct(ref);
			if (!condition && constructed()) destruct();
		}

		/**
		 * Access referenced object
		 */
		T       *operator -> ()       { _check_non_null(); return       _ptr; }
		T const *operator -> () const { _check_non_null(); return _const_ptr(); }

		T       &operator * ()       { _check_non_null(); return       *_ptr; }
		T const &operator * () const { _check_non_null(); return *_const_ptr(); }

		void print(Output &out) const
		{
			if (_ptr)
				_const_ptr()->print(out);
			else
				out.out_string("<unconstructed>");
		}
};


/**
 * Reconstructible object that holds no initially constructed object
 */
template <typename T>
struct Genode::Constructible : Reconstructible<T>
{
	Constructible()
	: Reconstructible<T>((typename Reconstructible<T>::Lazy *)nullptr) { }
};

#endif /* _INCLUDE__UTIL__RECONSTRUCTIBLE_H_ */
