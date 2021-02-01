/*
 * \brief  Lock dummy implementation
 * \author Stefan Kalkowski
 * \date   2016-09-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/lock.h>
#include <base/mutex.h>
#include <hw/assert.h>

Genode::Lock::Lock(Genode::Lock::State state)
: _state(state), _owner(nullptr) { }


void Genode::Lock::unlock()
{
	assert(_state == LOCKED);
	_state = UNLOCKED;
}


void Genode::Lock::lock()
{
	assert(_state == UNLOCKED);
	_state = LOCKED;
}


void Genode::Mutex::acquire()
{
	_lock.lock();
}


void Genode::Mutex::release()
{
	_lock.unlock();
}
