/*
 * \brief  Implementation of the Genode IPC-framework
 * \author Martin Stein
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

/* base-hw includes */
#include <kernel/interface.h>
#include <kernel/log.h>

namespace Hw { extern Genode::Untyped_capability _main_thread_cap; }

using namespace Genode;

enum
{
	/* size of the callee-local name of a targeted RPC object */
	RPC_OBJECT_ID_SIZE = sizeof(Native_thread_id),

	/*
	 * The RPC framework marshalls a return value into reply messages to
	 * deliver exceptions, wich occured during the RPC call to the caller.
	 * This defines the size of this value.
	 */
	RPC_RETURN_VALUE_SIZE = sizeof(umword_t),
};


/*****************************
 ** IPC marshalling support **
 *****************************/

void Ipc_ostream::_marshal_capability(Native_capability const &cap) {
	_snd_msg->cap_add(cap); }


void Ipc_istream::_unmarshal_capability(Native_capability &cap) {
	cap = _rcv_msg->cap_get(); }


/*****************
 ** Ipc_ostream **
 *****************/

Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = align_natural<unsigned>(RPC_OBJECT_ID_SIZE);
	_snd_msg->reset();
}


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait()
{
	/* FIXME: this shall be not supported */
	Kernel::pause_current_thread();
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg)
:
	Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()),
	Native_capability(Thread_base::myself() ? Thread_base::myself()->tid().cap
	                                        : Hw::_main_thread_cap),
	_rcv_msg(rcv_msg), _rcv_cs(-1)
{ _read_offset = align_natural<unsigned>(RPC_OBJECT_ID_SIZE); }


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	retry<Genode::Allocator::Out_of_memory>(
		[&] () {

			/* send request and receive corresponding reply */
			Thread_base::myself()->utcb()->copy_from(*_snd_msg, _write_offset);

			switch (Kernel::send_request_msg(Ipc_ostream::dst().dst(),
			                                 _rcv_msg->cap_rcv_window())) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default:
				_rcv_msg->reset();
				_snd_msg->reset();
				Thread_base::myself()->utcb()->copy_to(*_rcv_msg);

				/* reset unmarshaller */
				_write_offset = _read_offset =
					align_natural<unsigned>(RPC_OBJECT_ID_SIZE);
			}

		},
		[&] () { upgrade_pd_session_quota(3*4096); });
}


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg, unsigned short rcv_caps)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0) {
	rcv_msg->cap_rcv_window(rcv_caps); }


/****************
 ** Ipc_server **
 ****************/

Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg) :
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(), snd_msg),
	_reply_needed(false) { }


void Ipc_server::_prepare_next_reply_wait()
{
	/* now we have a request to reply */
	_reply_needed = true;

	/* leave space for RPC method return value */
	_write_offset = align_natural<unsigned>(RPC_OBJECT_ID_SIZE +
	                                        RPC_RETURN_VALUE_SIZE);

	/* reset unmarshaller */
	_read_offset  = align_natural<unsigned>(RPC_OBJECT_ID_SIZE);
}


void Ipc_server::_wait()
{
	retry<Genode::Allocator::Out_of_memory>(
		[&] () {

			/* receive request */
			switch (Kernel::await_request_msg(Msgbuf_base::MAX_CAP_ARGS)) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default:
				_rcv_msg->reset();
				Thread_base::myself()->utcb()->copy_to(*_rcv_msg);

				/* update server state */
				_prepare_next_reply_wait();
			}

		},
		[&] () { upgrade_pd_session_quota(3*4096); });
}


void Ipc_server::_reply()
{
	Thread_base::myself()->utcb()->copy_from(*_snd_msg, _write_offset);
	_snd_msg->reset();
	Kernel::send_reply_msg(0, false);
}


void Ipc_server::_reply_wait()
{
	/* if there is no reply, wait for request */
	if (!_reply_needed) {
		_wait();
		return;
	}

	retry<Genode::Allocator::Out_of_memory>(
		[&] () {
			/* send reply and receive next request */
			Thread_base::myself()->utcb()->copy_from(*_snd_msg, _write_offset);
			switch (Kernel::send_reply_msg(Msgbuf_base::MAX_CAP_ARGS, true)) {
			case -1: throw Blocking_canceled();
			case -2: throw Allocator::Out_of_memory();
			default:
				_rcv_msg->reset();
				_snd_msg->reset();
				Thread_base::myself()->utcb()->copy_to(*_rcv_msg);

				/* update server state */
				_prepare_next_reply_wait();
			}

		},
		[&] () { upgrade_pd_session_quota(3*4096); });
}
