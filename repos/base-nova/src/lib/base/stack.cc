/*
 * \brief  Stack-specific part of the thread library
 * \author Norman Feske
 * \author Alexander Boettcher
 * \author Martin Stein
 * \date   2010-01-19
 *
 * This part of the thread library is required by the IPC framework
 * also if no threads are used.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/env.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/stack.h>
#include <base/internal/stack_area.h>
#include <base/internal/native_utcb.h>

/* NOVA includes */
#include <nova/cap_map.h>

using namespace Genode;

extern addr_t __initial_sp;


/*******************
 ** local helpers **
 *******************/

Native_utcb * main_thread_utcb()
{
	using namespace Genode;
	return reinterpret_cast<Native_utcb *>(
	       stack_area_virtual_base() + stack_virtual_size() - Nova::PAGE_SIZE_BYTE);
}


addr_t main_thread_running_semaphore() { return Nova::SM_SEL_EC; }


class Initial_cap_range : public Cap_range
{
	private:

		enum { CAP_RANGE_START = 4096 };

	public:

		Initial_cap_range() : Cap_range(CAP_RANGE_START) { }
};


Initial_cap_range &initial_cap_range()
{
	static Initial_cap_range s;
	return s;
}


/*****************************
 ** Startup library support **
 *****************************/

void Genode::prepare_init_main_thread()
{
	cap_map().insert(initial_cap_range());

	/* for Core we can't perform the following code so early */
	if (!__initial_sp) {

		enum { CAP_RANGES = 32 };

		unsigned index = (unsigned)(initial_cap_range().base() +
		                            initial_cap_range().elements());

		static char local[CAP_RANGES][sizeof(Cap_range)];

		for (unsigned i = 0; i < CAP_RANGES; i++) {

			Cap_range &range = *reinterpret_cast<Cap_range *>(local[i]);
			construct_at<Cap_range>(&range, index);

			cap_map().insert(range);

			index = (unsigned)(range.base() + range.elements());
		}
	}
}


/************
 ** Thread **
 ************/

/* prevent the compiler from optimizing out the 'this' pointer check */
__attribute__((optimize("-fno-delete-null-pointer-checks")))
Native_utcb *Thread::utcb()
{
	/*
	 * If 'utcb' is called on the object returned by 'myself',
	 * the 'this' pointer may be NULL (if the calling thread is
	 * the main thread). Therefore we allow this special case
	 * here.
	 */
	if (this == 0) return main_thread_utcb();

	return &_stack->utcb();
}
