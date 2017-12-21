/*
 * \brief  Helper classes to make raw Nova IPC calls which can't be expressed
 *         via the Genode base RPC abstractions
 * \author Alexander Boettcher
 *
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* test specific includes */
#include "server.h"

using namespace Test;

long Test::cap_void_manual(Genode::Native_capability dst,
                           Genode::Native_capability arg1,
                           Genode::addr_t &local_reply)
{
	if (!arg1.valid())
		return Genode::Rpc_exception_code::INVALID_OBJECT;

	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	/* save original receive window */
	Nova::Crd orig_crd = utcb->crd_rcv;

	/* don't open receive window */
	utcb->crd_rcv = Nova::Obj_crd();
	/* not used on base-nova */
	utcb->msg()[0]  = 0;
	/* method number of RPC interface to be called on server side */
	utcb->msg()[1]  = 0;
	utcb->set_msg_word(2);

	Nova::Crd crd = Genode::Capability_space::crd(arg1);
	if (!utcb->append_item(crd, 0, false, false, false))
		return Genode::Rpc_exception_code::INVALID_OBJECT;

	Genode::uint8_t res = Nova::call(dst.local_name());

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	local_reply = utcb->msg()[1];
	return (res == (Genode::uint8_t)Nova::NOVA_OK && utcb->msg_words() == 3 && utcb->msg()[2])
	       ? utcb->msg()[0] : (long)Genode::Rpc_exception_code::INVALID_OBJECT;
}
