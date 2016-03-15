/*
 * \brief  Implementation of the Genode IPC-framework
 * \author Martin Stein
 * \author Norman Feske
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/ipc.h>
#include <base/allocator.h>
#include <base/thread.h>
#include <base/native_env.h>
#include <util/construct_at.h>
#include <util/retry.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/native_thread.h>
#include <base/internal/ipc_server.h>

/* base-hw includes */
#include <kernel/interface.h>
#include <kernel/log.h>

namespace Hw { extern Genode::Untyped_capability _main_thread_cap; }

using namespace Genode;


/*****************************
 ** IPC marshalling support **
 *****************************/

void Ipc_marshaller::insert(Native_capability const &cap) {
	_snd_msg.cap_add(cap); }


void Ipc_unmarshaller::extract(Native_capability &cap) {
	cap = _rcv_msg.cap_get(); }


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t rcv_caps)
{
	rcv_msg.cap_rcv_window(rcv_caps);

	Native_utcb &utcb = *Thread_base::myself()->utcb();

	retry<Genode::Allocator::Out_of_memory>(
		[&] () {

			/* send request and receive corresponding reply */
			Thread_base::myself()->utcb()->copy_from(snd_msg);

			switch (Kernel::send_request_msg(dst.dst(),
			                                 rcv_msg.cap_rcv_window())) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default:
				rcv_msg.reset();
				utcb.copy_to(rcv_msg);
			}

		},
		[&] () { upgrade_pd_session_quota(3*4096); });

	return Rpc_exception_code(utcb.exception_code());
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::reply()
{
	Thread_base::myself()->utcb()->copy_from(_snd_msg);
	_snd_msg.reset();
	Kernel::send_reply_msg(0, false);
}


void Ipc_server::reply_wait()
{
	Native_utcb &utcb = *Thread_base::myself()->utcb();

	retry<Genode::Allocator::Out_of_memory>(
		[&] () {
			int ret = 0;
			if (_reply_needed) {
				utcb.copy_from(_snd_msg);
				utcb.exception_code(_exception_code.value);
				ret = Kernel::send_reply_msg(Msgbuf_base::MAX_CAP_ARGS, true);
			} else {
				ret = Kernel::await_request_msg(Msgbuf_base::MAX_CAP_ARGS);
			}

			switch (ret) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default: break;
			}
		},
		[&] () { upgrade_pd_session_quota(3*4096); });

	_rcv_msg.reset();
	_snd_msg.reset();

	utcb.copy_to(_rcv_msg);
	_badge = utcb.destination();

	_reply_needed = true;
	_read_offset = _write_offset = 0;
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg),
	Native_capability(Thread_base::myself() ? Thread_base::myself()->native_thread().cap
	                                        : Hw::_main_thread_cap),
	_rcv_cs(cs)
{
	_snd_msg.reset();
}


Ipc_server::~Ipc_server() { }
