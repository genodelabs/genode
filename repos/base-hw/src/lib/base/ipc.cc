/*
 * \brief  Implementation of the Genode IPC-framework
 * \author Martin Stein
 * \author Norman Feske
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/ipc.h>
#include <base/allocator.h>
#include <base/thread.h>
#include <util/construct_at.h>
#include <util/retry.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/native_thread.h>
#include <base/internal/ipc_server.h>
#include <base/internal/native_env.h>
#include <base/internal/capability_space.h>

/* base-hw includes */
#include <kernel/interface.h>

namespace Hw { extern Genode::Untyped_capability _main_thread_cap; }

using namespace Genode;


/**
 * Copy data from the message buffer to UTCB
 */
static inline void copy_msg_to_utcb(Msgbuf_base const &snd_msg, Native_utcb &utcb)
{
	/* copy capabilities */
	size_t const num_caps = min((size_t)Msgbuf_base::MAX_CAPS_PER_MSG,
	                            snd_msg.used_caps());

	for (unsigned i = 0; i < num_caps; i++)
		utcb.cap_set(i, Capability_space::capid(snd_msg.cap(i)));

	utcb.cap_cnt(num_caps);

	/* copy data payload */
	size_t const data_size = min(snd_msg.data_size(),
	                             min(snd_msg.capacity(), utcb.capacity()));

	memcpy(utcb.data(), snd_msg.data(), data_size);

	utcb.data_size(data_size);
}


/**
 * Copy data from UTCB to the message buffer
 */
static inline void copy_utcb_to_msg(Native_utcb const &utcb, Msgbuf_base &rcv_msg)
{
	/* copy capabilities */
	size_t const num_caps = min((size_t)Msgbuf_base::MAX_CAPS_PER_MSG,
	                            utcb.cap_cnt());

	for (unsigned i = 0; i < num_caps; i++) {
		rcv_msg.cap(i) = Capability_space::import(utcb.cap_get(i));
		if (rcv_msg.cap(i).valid())
			Kernel::ack_cap(Capability_space::capid(rcv_msg.cap(i)));
	}

	rcv_msg.used_caps(num_caps);

	/* copy data payload */
	size_t const data_size = min(utcb.data_size(),
	                             min(utcb.capacity(), rcv_msg.capacity()));

	memcpy(rcv_msg.data(), utcb.data(), data_size);

	rcv_msg.data_size(data_size);
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t rcv_caps)
{
	Native_utcb &utcb = *Thread::myself()->utcb();

	retry<Genode::Allocator::Out_of_memory>(
		[&] () {

			copy_msg_to_utcb(snd_msg, *Thread::myself()->utcb());

			switch (Kernel::send_request_msg(Capability_space::capid(dst), rcv_caps)) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default:
				copy_utcb_to_msg(utcb, rcv_msg);
			}

		},
		[&] () { upgrade_capability_slab(); });

	return Rpc_exception_code(utcb.exception_code());
}


/****************
 ** IPC server **
 ****************/

void Genode::ipc_reply(Native_capability, Rpc_exception_code exc,
                       Msgbuf_base &snd_msg)
{
	Native_utcb &utcb = *Thread::myself()->utcb();
	copy_msg_to_utcb(snd_msg, utcb);
	utcb.exception_code(exc.value);
	snd_msg.reset();
	Kernel::send_reply_msg(0, false);
}


Genode::Rpc_request Genode::ipc_reply_wait(Reply_capability const &,
                                           Rpc_exception_code      exc,
                                           Msgbuf_base            &reply_msg,
                                           Msgbuf_base            &request_msg)
{
	Native_utcb &utcb = *Thread::myself()->utcb();

	retry<Genode::Allocator::Out_of_memory>(
		[&] () {
			int ret = 0;
			if (exc.value != Rpc_exception_code::INVALID_OBJECT) {
				copy_msg_to_utcb(reply_msg, utcb);
				utcb.exception_code(exc.value);
				ret = Kernel::send_reply_msg(Msgbuf_base::MAX_CAPS_PER_MSG, true);
			} else {
				ret = Kernel::await_request_msg(Msgbuf_base::MAX_CAPS_PER_MSG);
			}

			switch (ret) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default: break;
			}
		},
		[&] () { upgrade_capability_slab(); });

	copy_utcb_to_msg(utcb, request_msg);

	return Rpc_request(Native_capability(), utcb.destination());
}


Ipc_server::Ipc_server()
:
	Native_capability(Thread::myself() ? Thread::myself()->native_thread().cap
	                                   : Hw::_main_thread_cap)
{ }


Ipc_server::~Ipc_server() { }
