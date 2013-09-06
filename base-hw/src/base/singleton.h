/*
 * \brief  Helper for creating singleton objects
 * \author Norman Feske
 * \date   2013-05-14
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
 * instances without implicitly calling 'cmpxchg'. Because object creation is
 * not synchronized via a spin lock, it must not be used in scenarios where
 * multiple threads may contend.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SINGLETON_H_
#define _SINGLETON_H_

/* Genode includes */
#include <base/stdint.h>

inline void *operator new(Genode::size_t, void *at) { return at; }


template <typename T, int ALIGN = 2, typename... Args>
static inline T *unsynchronized_singleton(Args... args)
{
	/*
	 * Each instantiation of the function template with a different type 'T'
	 * yields a dedicated instance of the local static variables, thereby
	 * creating the living space for the singleton objects.
	 */
	static bool initialized;
	static int inst[sizeof(T)/sizeof(int) + 1] __attribute__((aligned(ALIGN)));

	/* execute constructor on first call */
	if (!initialized) {
		initialized = true;
		new (&inst) T(args...);
	}
	return reinterpret_cast<T *>(inst);
}

#endif /* _SINGLETON_H_ */
