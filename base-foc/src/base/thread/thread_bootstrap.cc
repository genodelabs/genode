/*
 * \brief  Fiasco.OC specific thread bootstrap code
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2011-01-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/thread.h>


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	using namespace Genode;
	enum { THREAD_CAP_ID = 1 };
	Cap_index * ci(cap_map()->insert(THREAD_CAP_ID, Fiasco::MAIN_THREAD_CAP));
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE] = (unsigned long)ci;
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_THREAD_OBJ] = 0;
}


void prepare_reinit_main_thread()
{
	using namespace Genode;
	construct_at<Capability_map>(cap_map());
	cap_idx_alloc()->reinit();
	prepare_init_main_thread();
}


/*****************
 ** Thread_base **
 *****************/

void Genode::Thread_base::_thread_bootstrap() { }


void Genode::Thread_base::_thread_start()
{
	using namespace Genode;

	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	Thread_base::myself()->_join_lock.unlock();
	Lock sleep_forever_lock(Lock::LOCKED);
	sleep_forever_lock.lock();
}
