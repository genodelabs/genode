/*
 * \brief  Implementation of the IPC API
 * \author Norman Feske
 * \date   2010-09-06
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>

/* kernel includes */
#include <kernel/syscalls.h>

using namespace Genode;


/***************
 ** Utilities **
 ***************/

template<typename T>
static unsigned size_to_size_in(unsigned s)
{
	return (unsigned)(s+sizeof(T)-1)/sizeof(T);
}


/**
 * Copy message registers from UTCB to destination message buffer
 */
static void copy_utcb_to_msgbuf(unsigned message_size,
                                Msgbuf_base *receive_buffer)
{
	if (!message_size) return;

	if (message_size > receive_buffer->size())
		message_size = receive_buffer->size();

	Cpu::word_t *message_buffer    = (Cpu::word_t*)receive_buffer->buf;
	Native_utcb *utcb              = Thread_base::myself()->utcb();
	unsigned     msg_size_in_words = size_to_size_in<Cpu::word_t>(message_size);

	for (unsigned i=0; i < msg_size_in_words; i++)
		message_buffer[i] = utcb->word[i];
}


/**
 * Copy message payload to UTCB message registers
 */
static void copy_msgbuf_to_utcb(Msgbuf_base *send_buffer,
                                unsigned message_size,
                                unsigned local_name)
{
	typedef Kernel::Utcb Utcb;

	if (!message_size) return;

	Native_utcb *utcb        = Thread_base::myself()->utcb();
	unsigned     header_size = sizeof(local_name);

	if (message_size + header_size > utcb->size()) {
		if (header_size > utcb->size())
			return;

		message_size = utcb->size()-header_size;
	}

	Cpu::word_t *message_buffer = (Cpu::word_t*)send_buffer->buf;
	unsigned msg_size_in_words  = size_to_size_in<Cpu::word_t>(message_size);
	unsigned h_size_in_words    = size_to_size_in<Cpu::word_t>(header_size);

	utcb->word[0] = local_name;

	for (unsigned i = h_size_in_words; i < msg_size_in_words; i++) {
		utcb->word[i] = message_buffer[i];
	}
}


/*****************
 ** Ipc_ostream **
 *****************/

Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg),
	_dst(dst)
{
	_write_offset = sizeof(umword_t);
}


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait() { Kernel::thread_sleep(); }


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg)
:
	Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()),
	Native_capability(Genode::my_thread_id(), 0),
	_rcv_msg(rcv_msg),
	_rcv_cs(-1)
{
	_read_offset = sizeof(umword_t);
}


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/


void Ipc_client::_call()
{
	unsigned request_size = _write_offset;
	copy_msgbuf_to_utcb(_snd_msg, request_size, Ipc_ostream::_dst.local_name());

	unsigned reply_size = Kernel::ipc_request(Ipc_ostream::_dst.dst(), request_size);

	copy_utcb_to_msgbuf(reply_size, _rcv_msg);

	/* reset marshalling / unmarshalling pointers */
	_write_offset = _read_offset=sizeof(umword_t);
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{ }


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	/* now we have a request to reply */
	_reply_needed = true;

	enum { RETURN_VALUE_SIZE = sizeof(umword_t) };
	_write_offset = sizeof(umword_t)+RETURN_VALUE_SIZE;

	_read_offset = sizeof(umword_t);
}


void Ipc_server::_wait()
{
	/* wait for new request */
	Cpu::size_t reply_size   = 0;
	Cpu::size_t request_size = Kernel::ipc_serve(reply_size);


	copy_utcb_to_msgbuf(request_size, _rcv_msg);
	_prepare_next_reply_wait();
}


void Ipc_server::_reply() { _prepare_next_reply_wait(); }


void Ipc_server::_reply_wait()
{
	unsigned reply_size = 0;
	if (_reply_needed) {
		reply_size = _write_offset;
		copy_msgbuf_to_utcb(_snd_msg, reply_size, Ipc_ostream::_dst.local_name());
	}

	unsigned request_size = Kernel::ipc_serve(reply_size);

	copy_utcb_to_msgbuf(request_size, _rcv_msg);
	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg)
:
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(my_thread_id(), 0), snd_msg),
	_reply_needed(false)
{ }

