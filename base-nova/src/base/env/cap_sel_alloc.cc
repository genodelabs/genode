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
 * Copyright (C) 2010-2013 Genode Labs GmbH
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
 * Return lock used to protect capability selector allocations
 */
static Genode::Lock &alloc_lock()
{
	static Genode::Lock alloc_lock_inst;
	return alloc_lock_inst;
}


addr_t Cap_selector_allocator::alloc(size_t num_caps_log2)
{
	Lock::Guard guard(alloc_lock());
	return Bit_allocator::alloc(num_caps_log2);
}


void Cap_selector_allocator::free(addr_t cap, size_t num_caps_log2)
{
	Lock::Guard guard(alloc_lock());
	Bit_allocator::free(cap, num_caps_log2);
}


Cap_selector_allocator::Cap_selector_allocator() : Bit_allocator()
{
	/* initialize lock */
	alloc_lock();

	/**
	 * The first selectors are reserved for exception portals and special
	 * purpose usage as defined in the nova syscall header file
	 */
	Bit_allocator::_reserve(0, Nova::NUM_INITIAL_PT_RESERVED);
}


namespace Genode {

	Cap_selector_allocator *cap_selector_allocator()
	{
		static Cap_selector_allocator inst;
		return &inst;
	}
}
