/*
 * \brief  Map rtld lock implementation to Genode primitives
 * \author Sebastian Sumpf
 * \date   2011-05-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "rtld_lock.h"
#include <base/lock.h>
#include <cpu/atomic.h>
#include <base/printf.h>
/**
 * Simple read/write lock implementation
 */
struct rtld_lock {

	Genode::Lock *_lock;
	Genode::Lock  _inc;

	int _read;

	rtld_lock(Genode::Lock *lock) : _lock(lock), _read(0) {}

	void read_lock()
	{
		Genode::Lock::Guard guard(_inc);
		if(++_read == 1)
			lock();
	}

	void read_unlock()
	{
		Genode::Lock::Guard guard(_inc);
		if (--_read == 0)
			unlock();
	}

	void lock() { _lock->lock(); }
	void unlock() { _lock->unlock(); }
};

static Genode::Lock _gbind_lock;
static Genode::Lock _gphdr_lock;

static rtld_lock _bind_lock(&_gbind_lock);
static rtld_lock _phdr_lock(&_gphdr_lock);

/* the two locks used within rtld */
rtld_lock_t rtld_bind_lock = &_bind_lock;
rtld_lock_t rtld_phdr_lock = &_phdr_lock;


extern "C" int rlock_acquire(rtld_lock_t lock)
{
	lock->read_lock();
	return 1;
}


extern "C" int wlock_acquire(rtld_lock_t lock)
{
	lock->lock();
	return 1;
}


extern "C" void rlock_release(rtld_lock_t lock, int locked)
{
	lock->read_unlock();
}


extern "C" void wlock_release(rtld_lock_t lock, int locked)
{
	lock->unlock();
}
