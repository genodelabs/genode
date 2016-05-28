/*
 * \brief  Implementation of the IPC API for NOVA
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>
#include <base/printf.h>

/* base-internal includes */
#include <base/internal/ipc.h>

using namespace Genode;


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t rcv_caps)
{
	Receive_window rcv_window;
	rcv_msg.reset();

	/* update receive window for capability selectors if needed */
	if (rcv_caps != ~0UL) {

		/* calculate max order of caps to be received during reply */
		unsigned short log2_max = rcv_caps ? log2(rcv_caps) : 0;
		if ((1U << log2_max) < rcv_caps) log2_max ++;

		rcv_window.rcv_wnd(log2_max);
	}

	Nova::Utcb &utcb = *(Nova::Utcb *)Thread::myself()->utcb();

	/* the protocol value is unused as the badge is delivered by the kernel */
	if (!copy_msgbuf_to_utcb(utcb, snd_msg, 0)) {
		PERR("could not setup IPC");
		throw Ipc_error();
	}

	/* if we can't setup receive window, die in order to recognize the issue */
	if (!rcv_window.prepare_rcv_window(utcb, dst.rcv_window()))
		/* printf doesn't work here since for IPC also rcv_prepare* is used */
		nova_die();

	/* establish the mapping via a portal traversal */
	uint8_t res = Nova::call(dst.local_name());
	if (res != Nova::NOVA_OK) {
		/* If an error occurred, reset word&item count (not done by kernel). */
		utcb.set_msg_word(0);
		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);
	}

	rcv_window.post_ipc(utcb, dst.rcv_window());

	/* handle malformed reply from a server */
	if (utcb.msg_words() < 1)
		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);

	return Rpc_exception_code(copy_utcb_to_msgbuf(utcb, rcv_window, rcv_msg));
}
