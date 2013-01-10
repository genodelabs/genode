/*
 * \brief  Genode C API memory functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/lock.h>
#include <base/env.h>
#include <base/allocator.h>


extern "C" {

	namespace Okl4 {
#include <genode/lock.h>
	}

	void *genode_alloc_lock()
	{
		return new (Genode::env()->heap()) Genode::Lock();
	}


	void genode_free_lock(void* lock)
	{
		if(lock)
			Genode::destroy<Genode::Lock>(Genode::env()->heap(),
			                              static_cast<Genode::Lock*>(lock));
	}


	void genode_lock(void* lock)
	{
		if(lock)
			static_cast<Genode::Lock*>(lock)->lock();
	}


	void genode_unlock(void* lock)
	{
		if(lock)
			static_cast<Genode::Lock*>(lock)->unlock();
	}

} // extern "C"
