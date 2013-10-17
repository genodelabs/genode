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
#include <kernel/syscalls.h>
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


/***************
 ** Utilities **
 ***************/

/**
 * Copy message from the callers UTCB to message buffer
 */
static void utcb_to_msgbuf(Msgbuf_base * const msgbuf)
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	size_t msg_size = utcb->ipc_msg_size();
	if (msg_size > msgbuf->size()) {
		kernel_log() << "oversized IPC message\n";
		msg_size = msgbuf->size();
	}
	memcpy(msgbuf->buf, utcb->ipc_msg_base(), msg_size);
}


/**
 * Copy message from message buffer to the callers UTCB
 */
static void msgbuf_to_utcb(Msgbuf_base * const msg_buf, size_t msg_size,
                           unsigned const local_name)
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	enum { NAME_SIZE = sizeof(local_name) };
	size_t const ipc_msg_size = msg_size + NAME_SIZE;
	if (ipc_msg_size > utcb->max_ipc_msg_size()) {
		kernel_log() << "oversized IPC message\n";
		msg_size = utcb->max_ipc_msg_size() - NAME_SIZE;
	}
	*(unsigned *)utcb->ipc_msg_base() = local_name;
	void * const utcb_msg = (void *)((addr_t)utcb->ipc_msg_base() + NAME_SIZE);
	void * const buf_msg  = (void *)((addr_t)msg_buf->buf + NAME_SIZE);
	memcpy(utcb_msg, buf_msg, msg_size);
	utcb->ipc_msg_size(ipc_msg_size);
}


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
	/* FIXME this shall be not supported */
	Kernel::pause_thread();
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
	using namespace Kernel;

	/* send request and receive reply */
	unsigned const local_name = Ipc_ostream::_dst.local_name();
	msgbuf_to_utcb(_snd_msg, _write_offset, local_name);
	int error = request_and_wait(Ipc_ostream::_dst.dst());
	if (error) { throw Blocking_canceled(); }
	utcb_to_msgbuf(_rcv_msg);

	/* reset unmarshaller */
	_write_offset = _read_offset = RPC_OBJECT_ID_SIZE;
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
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
	/* receive next request */
	int const error = Kernel::wait_for_request();
	if (!error) { utcb_to_msgbuf(_rcv_msg); }
	else {
		PERR("failed to receive request");
		throw Blocking_canceled();
	}
	/* update server state */
	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	utcb->ipc_msg_size(_write_offset);
	Kernel::reply(0);
}


void Ipc_server::_reply_wait()
{
	/* if there is no reply simply do wait for request */
	/* FIXME this shall be not supported */
	if (!_reply_needed) {
		_wait();
		return;
	}
	/* send reply and receive next request */
	unsigned const local_name = Ipc_ostream::_dst.local_name();
	msgbuf_to_utcb(_snd_msg, _write_offset, local_name);
	int const error = Kernel::reply(1);
	if (!error) { utcb_to_msgbuf(_rcv_msg); }
	else {
		PERR("failed to receive request");
		throw Blocking_canceled();
	}
	/* update server state */
	_prepare_next_reply_wait();
}

