/*
 * \brief  seL4-specific implementation of the Thread API
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/env.h>

#include <base/rpc_client.h>
#include <session/session.h>


using namespace Genode;


/**
 * Entry point entered by new threads
 */
//void Thread_base::_thread_start()
//{
//	PDBG("not implemented");
//}


/*****************
 ** Thread base **
 *****************/

void Thread_base::_init_platform_thread(size_t, Type type)
{
	PDBG("not implemented");
}


void Thread_base::_deinit_platform_thread()
{
	PDBG("not implemented");
}


void Thread_base::start()
{
	PDBG("not implemented");
}


void Thread_base::cancel_blocking()
{
	PDBG("not implemented");
}
