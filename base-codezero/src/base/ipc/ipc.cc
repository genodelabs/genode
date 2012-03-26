/*
 * \brief  Codezero implementation of the IPC API
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Codezero includes */
#include <codezero/syscalls.h>

/* Genode includes */
#include <base/ipc.h>
#include <base/printf.h>
#include <base/blocking.h>
#include <util/misc_math.h>

using namespace Genode;
using namespace Codezero;

enum { verbose_ipc = false };


/*****************
 ** Ipc_ostream **
 *****************/

void Ipc_ostream::_send()
{
	if (verbose_ipc)
		PDBG("thread %d sends IPC to %d, write_offset=%d",
		     thread_myself(), _dst.dst(), _write_offset);

	umword_t snd_size = min(_write_offset, (unsigned)L4_IPC_EXTENDED_MAX_SIZE);

	*(umword_t *)_snd_msg->addr() = _dst.local_name();

	int ret = l4_send_extended(_dst.dst(), L4_IPC_TAG_SYNC_EXTENDED,
	                           snd_size, _snd_msg->addr());
	if (ret < 0)
		PERR("l4_send_extended (to thread %d) returned ret=%d",
		     _dst.dst(), ret);

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
	umword_t *rcv_buf = (umword_t *)_rcv_msg->addr();
	umword_t rcv_size = min(_rcv_msg->size(), (unsigned)L4_IPC_EXTENDED_MAX_SIZE);

	if (verbose_ipc)
		PDBG("thread %d waits for IPC from %d, rcv_buf at %p, rcv_size=%d",
		     dst(), _rcv_cs, rcv_buf, (int)rcv_size);

	int ret = l4_receive_extended(_rcv_cs, rcv_size, rcv_buf);
	if (ret < 0)
		PERR("l4_receive_extended (from any) returned ret=%d", ret);

	if (verbose_ipc)
		PDBG("thread %d received IPC from %d",
		     dst(), l4_get_sender());

	_read_offset = sizeof(umword_t);
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg)
:
	Ipc_unmarshaller((char *)rcv_msg->addr(), rcv_msg->size()),
	Native_capability(thread_myself(), 0),
	_rcv_msg(rcv_msg)
{
	_rcv_cs = L4_ANYTHREAD;
	_read_offset = sizeof(umword_t);
}


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
#warning l4_sendrecv_extended is not yet implemented in l4lib/arch/syslib.h
	_send();
	_rcv_cs = Ipc_ostream::_dst.dst();
	_wait();
	_rcv_cs = L4_ANYTHREAD;

	_write_offset = _read_offset = sizeof(umword_t);
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

	/* leave space for return value at the beginning of the msgbuf */
	_write_offset = 2*sizeof(umword_t);

	/* receive buffer offset */
	_read_offset = sizeof(umword_t);
}


void Ipc_server::_wait()
{
	/* wait for new server request */
	try { Ipc_istream::_wait(); } catch (Blocking_canceled) { }

	/* define destination of next reply */
	Ipc_ostream::_dst = Native_capability(l4_get_sender(), badge());

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
