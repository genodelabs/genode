/*
 * \brief  Fiasco.OC specific thread bootstrap code
 * \author Stefan Kalkowski
 * \date   2011-01-20
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/sleep.h>
#include <base/thread.h>


void Genode::Thread_base::_thread_bootstrap() { }


void Genode::Thread_base::_thread_start()
{
	using namespace Genode;

	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	sleep_forever();
}


void Genode::Thread_base::_init_platform_thread() { }
