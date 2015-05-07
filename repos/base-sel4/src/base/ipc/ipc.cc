/*
 * \brief  seL4 implementation of the IPC API
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/printf.h>
#include <base/blocking.h>
#include <util/misc_math.h>

/* seL4 includes */
#include <sel4/interfaces/sel4_client.h>

using namespace Genode;


/*****************************
 ** IPC marshalling support **
 *****************************/

void Ipc_ostream::_marshal_capability(Native_capability const &cap)
{
	PDBG("not implemented");
}


void Ipc_istream::_unmarshal_capability(Native_capability &cap)
{
	PDBG("not implemented");
}


/*****************
 ** Ipc_ostream **
 *****************/

void Ipc_ostream::_send()
{
	PDBG("not implemented");

	_write_offset = sizeof(umword_t);
}



Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller((char *)snd_msg->addr(), snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = sizeof(umword_t);
}


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait()
{
	PDBG("not implemented");

	for (;;)
		seL4_Yield();


	_read_offset = sizeof(umword_t);
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg)
:
	Ipc_unmarshaller((char *)rcv_msg->addr(), rcv_msg->size()),
	_rcv_msg(rcv_msg)
{
	_read_offset = sizeof(umword_t);
}


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	PDBG("not implemented");

	_write_offset = _read_offset = sizeof(umword_t);
}


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg, unsigned short)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{ }


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	/* now we have a request to reply */
	_reply_needed = true;

	/* leave space for return value at the beginning of the msgbuf */
	_write_offset = 2*sizeof(umword_t);

	/* receive buffer offset */
	_read_offset = sizeof(umword_t);
}


void Ipc_server::_wait()
{
	/* wait for new server request */
	try { Ipc_istream::_wait(); } catch (Blocking_canceled) { }

	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	try { _send(); } catch (Ipc_error) { }

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	if (_reply_needed)
		_reply();

	_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg)
:
	Ipc_istream(rcv_msg), Ipc_ostream(Native_capability(), snd_msg),
	_reply_needed(false)
{ }
