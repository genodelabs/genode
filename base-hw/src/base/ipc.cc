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
#include <base/ipc.h>
#include <base/thread.h>

/* base-hw includes */
#include <kernel/interface.h>
#include <kernel/log.h>

using namespace Genode;

enum
{
	/* size of the callee-local name of a targeted RPC object */
	RPC_OBJECT_ID_SIZE = sizeof(umword_t),

	/*
	 * The RPC framework marshalls a return value into reply messages to
	 * deliver exceptions, wich occured during the RPC call to the caller.
	 * This defines the size of this value.
	 */
	RPC_RETURN_VALUE_SIZE = sizeof(umword_t),
};


/*****************
 ** Ipc_ostream **
 *****************/

Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = RPC_OBJECT_ID_SIZE;
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
	Native_capability(Genode::thread_get_my_native_id(), 0),
	_rcv_msg(rcv_msg), _rcv_cs(-1)
{ _read_offset = RPC_OBJECT_ID_SIZE; }


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	/* send request and receive corresponding reply */
	unsigned const local_name = Ipc_ostream::_dst.local_name();
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	utcb->message()->prepare_send(_snd_msg->buf, _write_offset, local_name);
	if (Kernel::send_request_msg(Ipc_ostream::_dst.dst())) {
		PERR("failed to receive reply");
		throw Blocking_canceled();
	}
	utcb->message()->finish_receive(_rcv_msg->buf, _rcv_msg->size());

	/* reset unmarshaller */
	_write_offset = _read_offset = RPC_OBJECT_ID_SIZE;
}


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg, unsigned short)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0) { }


/****************
 ** Ipc_server **
 ****************/

Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg) :
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(), snd_msg),
	_reply_needed(false)
{ }


void Ipc_server::_prepare_next_reply_wait()
{
	/* now we have a request to reply */
	_reply_needed = true;

	/* leave space for RPC method return value */
	_write_offset = RPC_OBJECT_ID_SIZE + RPC_RETURN_VALUE_SIZE;

	/* reset unmarshaller */
	_read_offset  = RPC_OBJECT_ID_SIZE;
}


void Ipc_server::_wait()
{
	/* receive request */
	if (Kernel::await_request_msg()) {
		PERR("failed to receive request");
		throw Blocking_canceled();
	}
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	utcb->message()->finish_receive(_rcv_msg->buf, _rcv_msg->size());

	/* update server state */
	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	unsigned const local_name = Ipc_ostream::_dst.local_name();
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	utcb->message()->prepare_send(_snd_msg->buf, _write_offset, local_name);
	Kernel::send_reply_msg(false);
}


void Ipc_server::_reply_wait()
{
	/* if there is no reply, wait for request */
	if (!_reply_needed) {
		_wait();
		return;
	}
	/* send reply and receive next request */
	unsigned const local_name = Ipc_ostream::_dst.local_name();
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	utcb->message()->prepare_send(_snd_msg->buf, _write_offset, local_name);
	if (Kernel::send_reply_msg(true)) {
		PERR("failed to receive request");
		throw Blocking_canceled();
	}
	utcb->message()->finish_receive(_rcv_msg->buf, _rcv_msg->size());

	/* update server state */
	_prepare_next_reply_wait();
}
