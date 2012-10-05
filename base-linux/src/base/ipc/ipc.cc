/*
 * \brief  Socket-based IPC implementation for Linux
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2011-10-11
 *
 * The current request message layout is:
 *
 *   long  server_local_name;
 *   long  client_thread_id;
 *   int   opcode;
 *   ...payload...
 *
 * Response messages look like this:
 *
 *   long  scratch_word;
 *   int   exc_code;
 *   ...payload...
 *
 * All fields are naturally aligned, i.e., aligend on 4 or 8 byte boundaries on
 * 32-bit resp. 64-bit systems.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>
#include <base/blocking.h>

/* Linux includes */
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include <linux_socket.h>
#include <linux_syscalls.h>


using namespace Genode;


/*****************
 ** Ipc_ostream **
 *****************/

/*
 * XXX This class will be removed soon.
 */

void Ipc_ostream::_prepare_next_send()
{
	PRAW("unexpected call to %s (%p)", __PRETTY_FUNCTION__, this);
}


void Ipc_ostream::_send()
{
	PRAW("unexpected call to %s (%p)", __PRETTY_FUNCTION__, this);
}


Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg):
	Ipc_marshaller(snd_msg->buf, snd_msg->size()), _snd_msg(snd_msg), _dst(dst)
{ }


/*****************
 ** Ipc_istream **
 *****************/

/*
 * XXX This class will be removed soon.
 */

void Ipc_istream::_prepare_next_receive()
{
	PRAW("unexpected call to %s (%p)", __PRETTY_FUNCTION__, this);
}


void Ipc_istream::_wait()
{
	PRAW("unexpected call to %s (%p)", __PRETTY_FUNCTION__, this);
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg)
: Ipc_unmarshaller(rcv_msg->buf, rcv_msg->size()),
  Native_capability(lx_gettid(), 0),
  _rcv_msg(rcv_msg), _rcv_cs(-1)
{ }


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_prepare_next_call()
{
	/* prepare next request in buffer */
	long local_name = Ipc_ostream::_dst.local_name();
	long tid        = Native_capability::dst();

	_write_offset = 0;
	_write_to_buf(local_name);
	_write_to_buf(tid);

	/* prepare response buffer */
	_read_offset = sizeof(long);
}


void Ipc_client::_call()
{
	if (Ipc_ostream::_dst.valid()) {
		lx_send_to(_rcv_cs, Ipc_ostream::_dst.dst(), "server",
		           _snd_msg->buf, _write_offset);

		lx_recv_from(_rcv_cs, _rcv_msg->buf, _rcv_msg->size());
	}
	_prepare_next_call();
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{
	_rcv_cs = lx_client_socket(Thread_base::myself());

	_prepare_next_call();
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	/* skip server-local name */
	_read_offset = sizeof(long);

	/* read client thread id from request buffer */
	long tid = 0;
	if (_reply_needed) {
		_read_from_buf(tid);
		Ipc_ostream::_dst = Native_capability(tid, 0); /* only _tid member is used */
	}

	/* prepare next reply */
	_write_offset   = 0;
	long local_name = Ipc_ostream::_dst.local_name();
	_write_to_buf(local_name);  /* XXX unused, needed by de/marshaller */

	/* leave space for exc code at the beginning of the msgbuf */
	_write_offset += align_natural(sizeof(int));
}


void Ipc_server::_wait()
{
	/* wait for new server request */
	try {
		lx_recv_from(_rcv_cs, _rcv_msg->buf, _rcv_msg->size());
	} catch (Blocking_canceled) { }

	/* now we have a request to reply, determine reply destination */
	_reply_needed = true;
	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	try {
		lx_send_to(_rcv_cs, Ipc_ostream::_dst.dst(), "client",
		           _snd_msg->buf, _write_offset);
	} catch (Ipc_error) { }

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	/* when first called, there was no request yet */
	if (_reply_needed)
		lx_send_to(_rcv_cs, Ipc_ostream::_dst.dst(), "client",
		           _snd_msg->buf, _write_offset);

	_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg),
  Ipc_ostream(Native_capability(), snd_msg), _reply_needed(false)
{
	_rcv_cs = lx_server_socket(Thread_base::myself());

	_prepare_next_reply_wait();
}
