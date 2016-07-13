/*
 * \brief  Utility for the manual placement of objects
 * \author Norman Feske
 * \date   2014-02-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__CONSTRUCT_AT_H_
#define _INCLUDE__UTIL__CONSTRUCT_AT_H_

#include <base/stdint.h>
#include <base/log.h>

namespace Genode {

	template <typename T, typename... ARGS>
	static inline T *construct_at(void *, ARGS &&...);
}


/**
 * Construct object of given type at a specific location
 *
 * \param T     object type
 * \param at    desired object location
 * \param args  list of arguments for the object constructor
 *
 * \return  typed object pointer
 *
 * We use move semantics (ARGS &&) because otherwise the compiler would create
 * a temporary copy of all arguments that have a reference type and use a
 * reference to this copy instead of the original within this function.
 *
 * There is a slight difference between the object that is constructed by this
 * function and a common object of the given type. If the destructor of the
 * given type or of any base of the given type is virtual, the vtable of the
 * returned object references an empty delete(void *) operator for that
 * destructor. However, this shouldn't be a problem as an object constructed by
 * this function should never get destructed implicitely or through a delete
 * expression.
 */
template <typename T, typename... ARGS>
static inline T * Genode::construct_at(void *at, ARGS &&... args)
{
	/**
	 * Utility to equip an existing type 'T' with a placement new operator
	 */
	struct Placeable : T
	{
		Placeable(ARGS &&... args) : T(args...) { }

		void * operator new (size_t, void *ptr) { return ptr; }
		void   operator delete (void *, void *) { }

		/**
		 * Standard delete operator
		 *
		 * As we explicitely define one version of the delete operator, the
		 * compiler won't implicitely define any delete version for this class.
		 * But if type T has a virtual destructor, the compiler implicitely
		 * defines a 'virtual ~Placeable()' which needs the following operator.
		 */
		void  operator delete (void *)
		{
			error("cxx: Placeable::operator delete (void *) not supported.");
		}
	};

	/*
	 * If the args input to this function contains rvalues, the compiler would
	 * use the according rvalue references as lvalues at the following call if
	 * we don't cast them back to rvalue references explicitely. We can not use
	 * lvalues here because the compiler can not bind them to rvalue references
	 * as expected by Placeable.
	 */
	return new (at) Placeable(static_cast<ARGS &&>(args)...);
}

#endif /* _INCLUDE__UTIL__CONSTRUCT_AT_H_ */
