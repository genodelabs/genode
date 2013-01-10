/*
 * \brief  Fiasco.OC specific thread bootstrap code
 * \author Stefan Kalkowski
 * \date   2011-01-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>


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


void Genode::Thread_base::_init_platform_thread() { }
