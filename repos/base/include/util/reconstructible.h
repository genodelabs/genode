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
#include <base/stdint.h>
#include <util/noncopyable.h>

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
 * \param  MT type
 */
template <typename MT>
class Genode::Reconstructible : Noncopyable
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

		class Deref_unconstructed_object { };

		/**
		 * Constructor
		 *
		 * The arguments are forwarded to the constructor of the embedded
		 * object.
		 */
		template <typename... ARGS>
		Reconstructible(ARGS &&... args)
		{
			_do_construct(args...);
		}

		~Reconstructible() { destruct(); }

		/**
		 * Construct new object in place
		 *
		 * If the 'Reconstructible' already hosts a constructed object, the old
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
		 * Construct or destruct volatile object according to 'condition'
		 */
		template <typename... ARGS>
		void conditional(bool condition, ARGS &&... args)
		{
			if (condition && !constructed())
				construct(args...);

			if (!condition && constructed())
				destruct();
		}

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
 * Reconstructible object that holds no initially constructed object
 */
template <typename MT>
struct Genode::Constructible : Reconstructible<MT>
{
	Constructible()
	: Reconstructible<MT>((typename Reconstructible<MT>::Lazy *)nullptr) { }
};

#endif /* _INCLUDE__UTIL__RECONSTRUCTIBLE_H_ */
