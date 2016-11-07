/*
 * \brief  Utility for manual in-place construction of objects
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__VOLATILE_OBJECT_H_
#define _INCLUDE__UTIL__VOLATILE_OBJECT_H_

#include <util/construct_at.h>
#include <base/stdint.h>
#include <util/noncopyable.h>

namespace Genode {
	template<typename> class Volatile_object;
	template<typename> class Lazy_volatile_object;
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
 * \param  MT type
 */
template <typename MT>
class Genode::Volatile_object : Noncopyable
{
	private:

		/**
		 * Static reservation of memory for the embedded object
		 */
		char _space[sizeof(MT)] __attribute__((aligned(sizeof(addr_t))));

		/**
		 * True if the volatile object contains a constructed object
		 */
		bool _constructed = false;

		template <typename... ARGS> void _do_construct(ARGS &&... args)
		{
			construct_at<MT>(_space, args...);
			_constructed = true;
		}

		MT       *_ptr()             { return reinterpret_cast<MT       *>(_space); }
		MT const *_const_ptr() const { return reinterpret_cast<MT const *>(_space); }

		void _check_constructed() const
		{
			if (!_constructed)
				throw Deref_unconstructed_object();
		}

	protected:

		/**
		 * Dummy type used as a hook for 'Lazy_volatile_object' to bypass the
		 * default constructor by invoking the 'Volatile_object(Lazy *)'
		 * constructor.
		 */
		struct Lazy { };

		/**
		 * Constructor that omits the initial construction of the object
		 */
		Volatile_object(Lazy *) { }

	public:

		class Deref_unconstructed_object { };

		/**
		 * Constructor
		 *
		 * The arguments are forwarded to the constructor of the embedded
		 * object.
		 */
		template <typename... ARGS>
		Volatile_object(ARGS &&... args)
		{
			_do_construct(args...);
		}

		~Volatile_object() { destruct(); }

		/**
		 * Construct new object in place
		 *
		 * If the 'Volatile_object' already hosts a constructed object, the old
		 * object will be destructed first.
		 */
		template <typename... ARGS>
		void construct(ARGS &&... args)
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
			_ptr()->~MT();

			_constructed = false;
		}

		/**
		 * Return true of volatile object contains a constructed object
		 */
		bool constructed() const { return _constructed; }

		/**
		 * Return true of volatile object contains a constructed object
		 *
		 * \deprecated use 'constructed' instead
		 */
		bool is_constructed() const { return constructed(); }

		/**
		 * Access contained object
		 */
		MT       *operator -> ()       { _check_constructed(); return       _ptr(); }
		MT const *operator -> () const { _check_constructed(); return _const_ptr(); }

		MT       &operator * ()       { _check_constructed(); return       *_ptr(); }
		MT const &operator * () const { _check_constructed(); return *_const_ptr(); }

		void print(Output &out) const
		{
			if (_constructed)
				_const_ptr()->print(out);
			else
				out.out_string("<unconstructed>");
		}
};


/**
 * Volatile object that holds no initially constructed object
 */
template <typename MT>
struct Genode::Lazy_volatile_object : Volatile_object<MT>
{
	template <typename... ARGS>
	Lazy_volatile_object(ARGS &&... args)
	:
		Volatile_object<MT>((typename Volatile_object<MT>::Lazy *)0)
	{ }
};

#endif /* _INCLUDE__UTIL__VOLATILE_OBJECT_H_ */
