/*
 * \brief  Standard implementation of pager object
 * \author Martin Stein
 * \date   2013-11-04
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>

using namespace Genode;


void Pager_object::wake_up()
{
	/* notify pager to wake up faulter */
	Msgbuf<16> snd, rcv;
	Native_capability pager = cap();
	Ipc_client ipc_client(pager, &snd, &rcv);
	ipc_client << this << IPC_CALL;
}
