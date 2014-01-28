/*
 * \brief  Thread bootstrap code
 * \author Christian Prochaska
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* Pistachio includes */
namespace Pistachio
{
	#include <l4/thread.h>
}

Genode::Native_thread_id main_thread_tid;


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	main_thread_tid = Pistachio::L4_Myself();
}

void prepare_reinit_main_thread() { prepare_init_main_thread(); }


/*****************
 ** Thread_base **
 *****************/

void Genode::Thread_base::_thread_bootstrap()
{
	_tid.l4id = Pistachio::L4_Myself();
}


void Genode::Thread_base::_init_platform_thread(Type type)
{
	if (type == NORMAL) { return; }
	_tid.l4id = main_thread_tid;
}
