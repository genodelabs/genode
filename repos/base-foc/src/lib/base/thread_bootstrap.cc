/*
 * \brief  Fiasco.OC specific thread bootstrap code
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2011-01-20
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <foc/native_capability.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/cap_map.h>


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	using namespace Genode;
	enum { THREAD_CAP_ID = 1 };
	Cap_index * ci(cap_map().insert(THREAD_CAP_ID, Fiasco::MAIN_THREAD_CAP));
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE] = (unsigned long)ci;
	Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_THREAD_OBJ] = 0;
}


void prepare_reinit_main_thread()
{
	using namespace Genode;
	construct_at<Capability_map>(&cap_map());
	cap_idx_alloc().reinit();
	prepare_init_main_thread();
}


/************
 ** Thread **
 ************/

void Genode::Thread::_thread_bootstrap() { }


void Genode::Thread::_thread_start()
{
	using namespace Genode;

	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();
	Thread::myself()->_join.wakeup();
	sleep_forever();
}
