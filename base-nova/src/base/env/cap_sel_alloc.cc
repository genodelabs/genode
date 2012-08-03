/*
 * \brief  Capability-selector allocator
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 *
 * This is a NOVA-specific addition to the process environment.
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
#include <nova/util.h>

using namespace Genode;


/**
 * First available capability selector for custom use
 *
 * Must be initialized by the startup code
 */
int __first_free_cap_selector;

/**
 * Low-level lock to protect the allocator
 *
 * We cannot use a normal Genode lock because this lock is used by code
 * executed prior the initialization of Genode.
 */
class Alloc_lock
{
	private:

		addr_t _sm_cap;

	public:

		/**
		 * Constructor
		 *
		 * \param sm_cap  capability selector for the used semaphore
		 */
		Alloc_lock() : _sm_cap(Nova::PD_SEL_CAP_LOCK) { }

		void lock()
		{
			if (Nova::sm_ctrl(_sm_cap, Nova::SEMAPHORE_DOWN))
				nova_die();
		}

		void unlock()
		{
			if (Nova::sm_ctrl(_sm_cap, Nova::SEMAPHORE_UP))
				nova_die();
		}
};


/**
 * Return lock used to protect capability selector allocations
 */
static Alloc_lock *alloc_lock()
{
	static Alloc_lock alloc_lock_inst;
	return &alloc_lock_inst;
}


addr_t Cap_selector_allocator::alloc(size_t num_caps_log2)
{
	alloc_lock()->lock();
	addr_t ret_base = Bit_allocator::alloc(num_caps_log2);
	alloc_lock()->unlock();
	return ret_base;
}

void Cap_selector_allocator::free(addr_t cap, size_t num_caps_log2)
{
	alloc_lock()->lock();
	Bit_allocator::free(cap, num_caps_log2);
	alloc_lock()->unlock();

}


Cap_selector_allocator::Cap_selector_allocator()
{
	/* initialize lock */
	alloc_lock();

	/* the first free selector is used for the lock */
	Bit_allocator::_reserve(0, __first_free_cap_selector + 1);
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
