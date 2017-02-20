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
#include <assert.h>

Genode::Cancelable_lock::Cancelable_lock(Genode::Cancelable_lock::State state)
: _state(state), _owner(nullptr) { }


void Genode::Cancelable_lock::unlock()
{
	assert(_state == LOCKED);
	_state = UNLOCKED;
}


void Genode::Cancelable_lock::lock()
{
	assert(_state == UNLOCKED);
	_state = LOCKED;
}
