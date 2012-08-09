/*
 * \brief  Socket-based IPC implementation for Linux
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2011-10-11
 *
 * The current request message layout is:
 *
 *   long  server_local_name;
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
#include <base/env.h>
#include <linux_cpu_session/linux_cpu_session.h>

/* Linux includes */
#include <linux_socket.h>
#include <linux_syscalls.h>


using namespace Genode;



Genode::Ep_socket_descriptor_registry *Genode::ep_sd_registry()
{
	static Genode::Ep_socket_descriptor_registry registry;
	return &registry;
}


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
	Native_capability(Dst(-1), 0),
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
	if (Ipc_ostream::_dst.valid())
		lx_call(Ipc_ostream::_dst.dst().socket, *_snd_msg, _write_offset, *_rcv_msg);

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

	/*
	 * Block infinitely if called from the main thread. This may happen if the
	 * main thread calls 'sleep_forever()'.
	 */
	if (!Thread_base::myself()) {
		struct timespec ts = { 1000, 0 };
		for (;;) lx_nanosleep(&ts, 0);
	}

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
		Ipc_ostream::_dst = Native_capability(Dst(reply_socket), DUMMY_LOCAL_NAME);

		_prepare_next_reply_wait();
	} catch (Blocking_canceled) { }
}


void Ipc_server::_reply()
{
	try {
		lx_reply(Ipc_ostream::_dst.dst().socket, *_snd_msg, _write_offset); }
	catch (Ipc_error) { }

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	/* when first called, there was no request yet */
	if (_reply_needed)
		lx_reply(Ipc_ostream::_dst.dst().socket, *_snd_msg, _write_offset);

	_wait();
}


namespace Genode {

	/*
	 * Helper for obtaining a bound and connected socket pair
	 *
	 * For core, the implementation is just a wrapper around
	 * 'lx_server_socket_pair()'. For all other processes, the implementation
	 * requests the socket pair from the Env::CPU session interface using a
	 * Linux-specific interface extension.
	 */
	Native_connection_state server_socket_pair();
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

	if (thread) {
		_rcv_cs = server_socket_pair();
		thread->tid().is_ipc_server = true;
	}

	/* override capability initialization performed by 'Ipc_istream' */
	*static_cast<Native_capability *>(this) =
		Native_capability(Native_capability::Dst(_rcv_cs.client_sd), 0);

	_prepare_next_reply_wait();
}
