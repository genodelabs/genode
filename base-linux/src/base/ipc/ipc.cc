/*
 * \brief  Socket-based IPC implementation for Linux
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2011-10-11
 *
 * We create two sockets under lx_rpath() for each thread: client and server
 * role. The naming is 'ep-<thread id>-<role>'. The socket descriptors are
 * cached in Thread_base::_tid.
 *
 * Currently two socket files are needed, as the client does not send the reply
 * socket access-rights in a combined message with the payload. In the future,
 * only server sockets must be bound in lx_rpath().
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
#include <base/snprintf.h>

/* Linux includes */
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include <linux_syscalls.h>
#include <linux_rpath.h>


extern "C" void wait_for_continue(void);
extern "C" int raw_write_str(const char *str);

#define PRAW(fmt, ...)                                       \
	do {                                                     \
		char str[128];                                       \
		snprintf(str, sizeof(str),                           \
		         ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__);   \
		raw_write_str(str);                                  \
	} while (0)


using namespace Genode;


/**
 * Utility: Create socket address for thread ID and role (client/server)
 */
static void lx_create_sockaddr(sockaddr_un *addr, long thread_id, char const *role)
{
	addr->sun_family = AF_UNIX;
	Genode::snprintf(addr->sun_path, sizeof(addr->sun_path), "%s/ep-%ld-%s",
	                 lx_rpath(), thread_id, role);
}


/**
 * Utility: Create a socket descriptor and file for given thread and role
 */
static int create_socket(long thread_id, char const *role)
{
	int sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sd < 0) return -1;

	sockaddr_un addr;
	lx_create_sockaddr(&addr, thread_id, role);

	/* make sure bind succeeds */
	lx_unlink(addr.sun_path);

	if (lx_bind(sd, (sockaddr *)&addr, sizeof(addr)) < 0)
		return -2;

	return sd;
}


/**
 * Utility: Unlink socket file and close descriptor
 *
 * XXX Currently, socket destruction is missing. The client socket could be
 *     used from multiple Ipc_client objects. A safe destruction would need
 *     reference counting.
 */
//static void destroy_socket(int sd, long thread_id, char const *role)
//{
//	sockaddr_un addr;
//	lx_create_sockaddr(&addr, thread_id, role);
//
//	lx_unlink(addr.sun_path);
//	lx_close(sd);
//}


/**
 * Get client-socket descriptor for main thread
 */
static int main_client_socket()
{
	static int sd = create_socket(lx_gettid(), "client");

	return sd;
}


/**
 * Utility: Get server socket for given thread
 */
static int server_socket(Thread_base *thread)
{
	/*
	 * Main thread uses Ipc_server for sleep_forever() only.
	 */
	if (!thread)
		return lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

	if (thread->tid().server == -1)
		thread->tid().server = create_socket(thread->tid().tid, "server");
	return thread->tid().server;
}


/**
 * Utility: Get client socket for given thread
 */
static int client_socket(Thread_base *thread)
{
	if (!thread) return main_client_socket();

	if (thread->tid().client == -1)
		thread->tid().client = create_socket(thread->tid().tid, "client");
	return thread->tid().client;
}


/**
 * Utility: Send message to thread via given socket descriptor
 */
static void send_to(int sd, long thread_id, char const *target_role,
                    void *msg, Genode::size_t msg_len)
{
	sockaddr_un addr;
	lx_create_sockaddr(&addr, thread_id, target_role);

	int res = lx_sendto(sd, msg, msg_len, 0, (sockaddr *)&addr, sizeof(addr));
	if (res < 0) {
		PRAW("Send error: %d with %s in %d", res, addr.sun_path, lx_gettid());
		wait_for_continue();
		throw Ipc_error();
	}
}


/**
 * Utility: Receive message via given socket descriptor
 */
static void recv_from(int sd, void *buf, Genode::size_t buf_len)
{
	socklen_t fromlen;
	int res = lx_recvfrom(sd, buf, buf_len, 0, 0, &fromlen);
	if (res < 0) {
		if ((-res) == EINTR)
			throw Blocking_canceled();
		else {
			PRAW("Recv error: %d in %d", res, lx_gettid());
			wait_for_continue();
			throw Ipc_error();
		}
	}
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
	long local_name = _dst.local_name();
	long tid        = Native_capability::tid();

	_write_offset = 0;
	_write_to_buf(local_name);
	_write_to_buf(tid);

	/* prepare response buffer */
	_read_offset = sizeof(long);
}


void Ipc_client::_call()
{
	if (_dst.valid()) {
		send_to(_rcv_cs, _dst.tid(), "server",
		        _snd_msg->buf, _write_offset);

		recv_from(_rcv_cs, _rcv_msg->buf, _rcv_msg->size());
	}
	_prepare_next_call();
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{
	_rcv_cs = client_socket(Thread_base::myself());

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
	long tid;
	if (_reply_needed) {
		_read_from_buf(tid);
		_dst = Native_capability(tid, 0); /* only _tid member is used */
	}

	/* prepare next reply */
	_write_offset   = 0;
	long local_name = _dst.local_name();
	_write_to_buf(local_name);  /* XXX unused, needed by de/marshaller */

	/* leave space for exc code at the beginning of the msgbuf */
	_write_offset += align_natural(sizeof(int));
}


void Ipc_server::_wait()
{
	/* wait for new server request */
	try {
		recv_from(_rcv_cs, _rcv_msg->buf, _rcv_msg->size());
	} catch (Blocking_canceled) { }

	/* now we have a request to reply, determine reply destination */
	_reply_needed = true;
	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	try {
		send_to(_rcv_cs, _dst.tid(), "client", _snd_msg->buf, _write_offset);
	} catch (Ipc_error) { }

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	/* when first called, there was no request yet */
	if (_reply_needed)
		send_to(_rcv_cs, _dst.tid(), "client", _snd_msg->buf, _write_offset);

	_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg),
  Ipc_ostream(Native_capability(), snd_msg), _reply_needed(false)
{
	_rcv_cs = server_socket(Thread_base::myself());

	_prepare_next_reply_wait();
}
