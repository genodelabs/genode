/*
 * \brief  Capability-selector allocator
 * \author Stefan Kalkowski
 * \date   2010-12-06
 *
 * This is a Fiasco.OC-specific addition to the process enviroment.
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cap_sel_alloc.h>
#include <base/native_types.h>
#include <base/printf.h>
#include <cpu/atomic.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/consts.h>
}

using namespace Genode;

/**
 * First available capability selector for custom use
 *
 * Must be initialized by the startup code
 */
static unsigned long __first_free_cap_selector =
	Fiasco::Fiasco_capability::USER_BASE_CAP;

/**
 * Low-level lock to protect the allocator
 *
 * We cannot use a normal Genode lock because this lock is used by code
 * executed prior the initialization of Genode.
 */
class Alloc_lock
{
	private:

		int _state;

	public:

		enum State { LOCKED, UNLOCKED };

		/**
		 * Constructor
		 */
		Alloc_lock() : _state(UNLOCKED) {}

		void lock()
		{
			while (!Genode::cmpxchg(&_state, UNLOCKED, LOCKED))
				Fiasco::l4_ipc_sleep(Fiasco::l4_ipc_timeout(0, 0, 500, 0));
		}

		void unlock() { _state = UNLOCKED; }
};


/**
 * Return lock used to protect capability selector allocations
 */
static Alloc_lock *alloc_lock()
{
	static Alloc_lock alloc_lock_inst;
	return &alloc_lock_inst;
}


addr_t Capability_allocator::alloc(size_t num_caps)
{
	alloc_lock()->lock();
	int ret_base = _cap_idx;
	_cap_idx += num_caps * Fiasco::L4_CAP_SIZE;
	alloc_lock()->unlock();
	return ret_base;
}


void Capability_allocator::free(addr_t cap, size_t num_caps_log2)
{
//	PWRN("Not yet implemented!");
}


Capability_allocator::Capability_allocator()
: _cap_idx(__first_free_cap_selector)
{
	/* initialize lock */
	alloc_lock();
}


Capability_allocator* Capability_allocator::allocator()
{
	static Capability_allocator inst;
	return &inst;
}
