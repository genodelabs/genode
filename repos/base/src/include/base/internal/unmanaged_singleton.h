/*
 * \brief  Singleton objects that aren't implicitly constructed or destructed
 * \author Norman Feske
 * \author Martin Stein
 * \date   2013-12-04
 *
 * Before enabling the MMU on ARM, the 'cmpxchg' implementation is not always
 * guaranteed to work. For example, on the Raspberry Pi, the 'ldrex' as used by
 * 'cmpxchg' causes the machine to reboot. After enabling the MMU, everything
 * is fine. Hence, we need to avoid executing 'cmpxchg' prior this point.
 * Unfortunately, 'cmpxchg' is implicitly called each time when creating a
 * singleton object via a local-static object pattern. In this case, the
 * compiler generates code that calls the '__cxa_guard_acquire' function of the
 * C++ runtime, which, in turn, relies 'cmpxchg' for synchronization.
 *
 * The utility provided herein is an alternative way to create single object
 * instances without implicitly calling 'cmpxchg'. Furthermore, the created
 * objects are not destructed automatically at program exit which is useful
 * because it prevents the main thread of a program from destructing the
 * enviroment it needs to finish program close-down. Because object creation
 * is not synchronized via a spin lock, it must not be used in scenarios where
 * multiple threads may contend.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__UNMANAGED_SINGLETON_H_
#define _INCLUDE__BASE__INTERNAL__UNMANAGED_SINGLETON_H_

/* Genode includes */
#include <base/stdint.h>

/**
 * Placement new operator
 *
 * \param p  destination address
 */
inline void * operator new(__SIZE_TYPE__, void * p) { return p; }

/**
 * Helper class for the use of unmanaged_singleton with the singleton pattern
 *
 * If a class wants to make its constructor private to force the singleton
 * pattern, it can declare this class as friend to be able to still use the
 * unmanaged_singleton template.
 */
struct Unmanaged_singleton_constructor
{
	/**
	 * Call the constructor of 'T' with arguments 'args' at 'dst'
	 */
	template <typename T, typename... ARGS>
	static void call(char * const dst, ARGS &&... args) { new (dst) T(args...); }
};

/**
 * Create a singleton object that isn't implicitly constructed or destructed
 *
 * \param T          object type
 * \param ALIGNMENT  object alignment
 * \param ARGS       arguments to the object constructor
 *
 * \return  object pointer
 */
template <typename T, int ALIGNMENT = sizeof(Genode::addr_t), typename... ARGS>
static inline T * unmanaged_singleton(ARGS &&... args)
{
	/*
	 * Each instantiation of the function template with a different type 'T'
	 * yields a dedicated instance of the local static variables, thereby
	 * creating the living space for the singleton objects.
	 */
	enum { OBJECT_SIZE = sizeof(T) / sizeof(char) + 1 };
	static bool object_constructed = false;
	static char object_space[OBJECT_SIZE] __attribute__((aligned(ALIGNMENT)));

	/* execute constructor on first call */
	if (!object_constructed) {
		object_constructed = true;
		Unmanaged_singleton_constructor::call<T>(object_space, args...);
	}
	return reinterpret_cast<T *>(object_space);
}

#endif /* _INCLUDE__BASE__INTERNAL__UNMANAGED_SINGLETON_H_ */
