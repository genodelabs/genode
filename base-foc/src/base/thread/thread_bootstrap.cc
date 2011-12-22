/*
 * \brief  Fiasco.OC specific thread bootstrap code
 * \author Stefan Kalkowski
 * \date   2011-01-20
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/ipc.h>
#include <base/sleep.h>
#include <base/thread.h>

namespace Fiasco {
#include <l4/sys/utcb.h>
}

void Genode::Thread_base::_thread_bootstrap()
{
	using namespace Genode;
	using namespace Fiasco;

	/* first, receive my own gate-capability and badge from starter thread */
	addr_t        thread_base = 0;
	unsigned long my_badge    = 0;
	Msgbuf<128>   snd_msg, rcv_msg;
	Ipc_server    srv(&snd_msg, &rcv_msg);
	srv >> IPC_WAIT >> thread_base >> my_badge << IPC_REPLY;

	/* store both values in user-defined section of the UTCB */
	l4_utcb_tcr()->user[UTCB_TCR_BADGE]      = my_badge;
	l4_utcb_tcr()->user[UTCB_TCR_THREAD_OBJ] = thread_base;
}


void Genode::Thread_base::_thread_start()
{
	using namespace Genode;

	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	sleep_forever();
}


void Genode::Thread_base::_init_platform_thread() { }
