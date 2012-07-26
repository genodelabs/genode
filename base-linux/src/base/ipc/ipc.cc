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
#include <sys/types.h>
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
:
	Ipc_unmarshaller(rcv_msg->buf, rcv_msg->size()),
	Native_capability(Dst(lx_gettid(), -1), 0),
	_rcv_msg(rcv_msg)
{ }


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_prepare_next_call()
{
	/* prepare next request in buffer */
	long const local_name = Ipc_ostream::_dst.local_name();

	_write_offset = 0;
	_write_to_buf(local_name);

	/* prepare response buffer */
	_read_offset = sizeof(long);

	_snd_msg->reset_caps();
}


void Ipc_client::_call()
{
	if (Ipc_ostream::_dst.valid()) {
		_snd_msg->used_size(_write_offset);
		lx_call(Ipc_ostream::_dst.dst().tid, *_snd_msg, *_rcv_msg);
	}
	_prepare_next_call();
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{
	_prepare_next_call();
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	/* skip server-local name */
	_read_offset = sizeof(long);

	/* prepare next reply */
	_write_offset   = 0;
	long local_name = Ipc_ostream::_dst.local_name();
	_write_to_buf(local_name);  /* XXX unused, needed by de/marshaller */

	/* leave space for exc code at the beginning of the msgbuf */
	_write_offset += align_natural(sizeof(int));

	/* reset capability slots of send message buffer */
	_snd_msg->reset_caps();
}


void Ipc_server::_wait()
{
	_reply_needed = true;

	try {
		int const reply_socket = lx_wait(_rcv_cs, *_rcv_msg);

		/*
		 * Remember reply capability
		 *
		 * The 'local_name' of a capability is meaningful for addressing server
		 * objects only. Because a reply capabilities does not address a server
		 * object, the 'local_name' is meaningless.
		 */
		enum { DUMMY_LOCAL_NAME = -1 };

		typedef Native_capability::Dst Dst;
		enum { DUMMY_TID = -1 };
		Dst dst(DUMMY_TID, reply_socket);
		Ipc_ostream::_dst = Native_capability(dst, DUMMY_LOCAL_NAME);

		_prepare_next_reply_wait();
	} catch (Blocking_canceled) { }
}


void Ipc_server::_reply()
{
	try {
		_snd_msg->used_size(_write_offset);
		lx_reply(Ipc_ostream::_dst.dst().socket, *_snd_msg);
	} catch (Ipc_error) { }

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	/* when first called, there was no request yet */
	if (_reply_needed) {
		_snd_msg->used_size(_write_offset);
		lx_reply(Ipc_ostream::_dst.dst().socket, *_snd_msg);
	}

	_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
:
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(), snd_msg), _reply_needed(false)
{
	Thread_base *thread = Thread_base::myself();

	/*
	 * If 'thread' is 0, the constructor was called by the main thread. By
	 * definition, main is never an RPC entrypoint. However, the main thread
	 * may call 'sleep_forever()', which instantiates 'Ipc_server'.
	 */

	if (thread && thread->tid().is_ipc_server) {
		PRAW("unexpected multiple instantiation of Ipc_server by one thread");
		struct Ipc_server_multiple_instance { };
		throw Ipc_server_multiple_instance();
	}

	_rcv_cs = lx_server_socket(Thread_base::myself());
	if (_rcv_cs < 0) {
		PRAW("lx_server_socket failed (error %d)", _rcv_cs);
		struct Ipc_socket_creation_failed { };
		throw Ipc_socket_creation_failed();
	}

	if (thread)
		thread->tid().is_ipc_server = true;

	/* override capability initialization performed by 'Ipc_istream' */
	*static_cast<Native_capability *>(this) =
		Native_capability(Native_capability::Dst(lx_gettid(), _rcv_cs), 0);

	_prepare_next_reply_wait();
}
