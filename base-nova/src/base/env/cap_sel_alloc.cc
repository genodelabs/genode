/*
 * \brief  Capability-selector allocator
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2010-01-19
 *
 * This is a NOVA-specific addition to the process enviroment.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cap_sel_alloc.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;


/**
 * First available capability selector for custom use
 *
 * Must be initialized by the startup code
 */
int __first_free_cap_selector;
int __local_pd_sel;

/**
 * Low-level lock to protect the allocator
 *
 * We cannot use a normal Genode lock because this lock is used by code
 * executed prior the initialization of Genode.
 */
class Alloc_lock
{
	private:

		int _sm_cap;

	public:

		/**
		 * Constructor
		 *
		 * \param sm_cap  capability selector for the used semaphore
		 */
		Alloc_lock(int sm_cap) : _sm_cap(sm_cap)
		{
			Nova::create_sm(_sm_cap, __local_pd_sel, 1);
		}

		void lock() { Nova::sm_ctrl(_sm_cap, Nova::SEMAPHORE_DOWN); }

		void unlock() { Nova::sm_ctrl(_sm_cap, Nova::SEMAPHORE_UP); }
};


/**
 * Return lock used to protect capability selector allocations
 */
static Alloc_lock *alloc_lock()
{
	static Alloc_lock alloc_lock_inst(__first_free_cap_selector);
	return &alloc_lock_inst;
}


static int _cap_free;


addr_t Cap_selector_allocator::alloc(size_t num_caps_log2)
{
	alloc_lock()->lock();
	int num_caps = 1 << num_caps_log2;
	int ret_base = (_cap_free + num_caps - 1) & ~(num_caps - 1);
	_cap_free = ret_base + num_caps;
	alloc_lock()->unlock();
	return ret_base;
}


void Cap_selector_allocator::free(addr_t cap, size_t num_caps_log2)
{
	/*
	 * We don't free capability selectors because revoke is not supported
	 * on NOVA yet, anyway.
	 */
}


unsigned Cap_selector_allocator::pd_sel() { return __local_pd_sel; }


Cap_selector_allocator::Cap_selector_allocator()
{
	/* initialize lock */
	alloc_lock();

	/* the first free selector is used for the lock */
	_cap_free = __first_free_cap_selector + 1;
}


namespace Genode {

	/**
	 * This function must not be called prior the initialization of
	 * '__first_free_cap_selector'.
	 */
	Cap_selector_allocator *cap_selector_allocator()
	{
		static Cap_selector_allocator inst;
		return &inst;
	}
}
