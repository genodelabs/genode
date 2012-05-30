/*
 * \brief  Implementation of the Genode IPC-framework
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
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
 * Translate byte size 's' to size in words
 */
static unsigned long size_in_words(unsigned long const s)
{ return (s + sizeof(unsigned long) - 1) / sizeof(unsigned long); }


/**
 * Copy message payload to message buffer
 */
static void copy_utcb_to_msgbuf(Msgbuf_base * const receive_buffer,
                                unsigned long const message_size)
{
	/* log data that is received via IPC */
	enum { VERBOSE = 0 };

	/* get pointers and message attributes */
	Native_utcb * const   utcb          = Thread_base::myself()->utcb();
	unsigned long * const msgbuf        = (unsigned long *)receive_buffer->buf;
	unsigned long const   message_wsize = size_in_words(message_size);

	/* assertions, avoid 'printf' in here, it may lead to infinite recursion */
	if (message_wsize > size_in_words(utcb->size()))
	{
		kernel_log() << __PRETTY_FUNCTION__ << ": Oversized message\n";
		while (1) ;
	}
	/* fill message buffer with message */
	for (unsigned i=0; i < message_wsize; i++)
		msgbuf[i] = *utcb->word(i);
}


/**
 * Copy message payload to the UTCB
 */
static void copy_msgbuf_to_utcb(Msgbuf_base * const send_buffer,
                                unsigned long const message_size,
                                unsigned long const local_name)
{
	/* log data that is send via IPC */
	enum { VERBOSE = 0 };

	/* get pointers and message attributes */
	Native_utcb * const   utcb          = Thread_base::myself()->utcb();
	unsigned long * const msgbuf        = (unsigned long *)send_buffer->buf;
	unsigned long const   message_wsize = size_in_words(message_size);

	/* assertions, avoid 'printf' in here, it may lead to infinite recursion */
	if (message_wsize > size_in_words(utcb->size()))
	{
		kernel_log() << __PRETTY_FUNCTION__ << ": Oversized message\n";
		while (1) ;
	}
	/* address message to an object that the targeted thread knows */
	*utcb->word(0) = local_name;

	/* write message payload */
	for (unsigned long i = 1; i < message_wsize; i++)
		*utcb->word(i) = msgbuf[i];
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
	copy_msgbuf_to_utcb(_snd_msg, _write_offset,
	                    Ipc_ostream::_dst.local_name());
	size_t const s = request_and_wait(Ipc_ostream::_dst.dst(), _write_offset);
	copy_utcb_to_msgbuf(_rcv_msg, s);

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
	copy_utcb_to_msgbuf(_rcv_msg, Kernel::wait_for_request());

	/* update server state */
	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Unexpected call\n";
	while (1) ;
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
	copy_msgbuf_to_utcb(_snd_msg, _write_offset,
	                    Ipc_ostream::_dst.local_name());
	copy_utcb_to_msgbuf(_rcv_msg, Kernel::reply_and_wait(_write_offset));

	/* update server state */
	_prepare_next_reply_wait();
}

