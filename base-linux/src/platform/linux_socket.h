/*
 * \brief  Linux socket utilities
 * \author Christian Helmuth
 * \date   2012-01-17
 *
 * We create one socket under lx_rpath() for each 'Ipc_server'. The naming is
 * 'ep-<thread id>-<role>'.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM__LINUX_SOCKET_H_
#define _PLATFORM__LINUX_SOCKET_H_

/* Genode includes */
#include <base/ipc_generic.h>
#include <base/thread.h>
#include <base/blocking.h>
#include <base/snprintf.h>

#include <linux_rpath.h>
#include <linux_syscalls.h>


extern "C" void wait_for_continue(void);
extern "C" int raw_write_str(const char *str);

#define PRAW(fmt, ...)                                             \
	do {                                                           \
		char str[128];                                             \
		Genode::snprintf(str, sizeof(str),                         \
		                 ESC_ERR fmt ESC_END "\n", ##__VA_ARGS__); \
		raw_write_str(str);                                        \
	} while (0)


/**
 * Utility: Create socket address for server entrypoint atthread ID
 */
static void lx_create_server_addr(sockaddr_un *addr, long thread_id)
{
	addr->sun_family = AF_UNIX;
	Genode::snprintf(addr->sun_path, sizeof(addr->sun_path), "%s/ep-%ld",
	 lx_rpath(), thread_id);
}


namespace {

	/**
	 * Message object encapsulating data for sendmsg/recvmsg
	 */
	struct Message
	{
		public:

			enum { MAX_SDS_PER_MSG = Genode::Msgbuf_base::MAX_CAPS_PER_MSG };

		private:

			msghdr      _msg;
			sockaddr_un _addr;
			iovec       _iovec;
			char        _cmsg_buf[CMSG_SPACE(MAX_SDS_PER_MSG*sizeof(int))];

			unsigned _num_sds;

		public:

			Message(long server_thread_id = -1) : _num_sds(0)
			{
				memset(&_msg, 0, sizeof(_msg));

				if (server_thread_id != -1) {
					/* initialize receiver */
					lx_create_server_addr(&_addr, server_thread_id);

					_msg.msg_name    = &_addr;
					_msg.msg_namelen = sizeof(_addr);
				}

				/* initialize control message */
				struct cmsghdr *cmsg;

				_msg.msg_control     = _cmsg_buf;
				_msg.msg_controllen  = sizeof(_cmsg_buf);  /* buffer space available */
				_msg.msg_flags      |= MSG_CMSG_CLOEXEC;
				cmsg                 = CMSG_FIRSTHDR(&_msg);
				cmsg->cmsg_len       = CMSG_LEN(0);
				cmsg->cmsg_level     = SOL_SOCKET;
				cmsg->cmsg_type      = SCM_RIGHTS;
				_msg.msg_controllen  = cmsg->cmsg_len;     /* actual cmsg length */
			}

			msghdr * msg() { return &_msg; }

			void buffer(void *buffer, size_t buffer_len)
			{
				/* initialize iovec */
				_msg.msg_iov    = &_iovec;
				_msg.msg_iovlen = 1;

				_iovec.iov_base = buffer;
				_iovec.iov_len  = buffer_len;
			}

			void marshal_socket(int sd)
			{
				*((int *)CMSG_DATA((cmsghdr *)_cmsg_buf) + _num_sds) = sd;

				_num_sds++;

				struct cmsghdr *cmsg = CMSG_FIRSTHDR(&_msg);
				cmsg->cmsg_len       = CMSG_LEN(_num_sds*sizeof(int));
				_msg.msg_controllen  = cmsg->cmsg_len;     /* actual cmsg length */
			}

			void accept_sockets(int num_sds)
			{
				struct cmsghdr *cmsg = CMSG_FIRSTHDR(&_msg);
				cmsg->cmsg_len       = CMSG_LEN(num_sds*sizeof(int));
				_msg.msg_controllen  = cmsg->cmsg_len;     /* actual cmsg length */
			}

			int unmarshal_socket()
			{
				int ret = *((int *)CMSG_DATA((cmsghdr *)_cmsg_buf) + _num_sds);

				_num_sds++;

				return ret;
			}

			/* XXX only for debugging */
			int socket_at_index(int index)
			{
				return *((int *)CMSG_DATA((cmsghdr *)_cmsg_buf) + index);
			}

			unsigned num_sockets() const
			{
				struct cmsghdr *cmsg = CMSG_FIRSTHDR(&_msg);

				if (!cmsg)
					return 0;

				return (cmsg->cmsg_len - CMSG_ALIGN(sizeof(cmsghdr)))/sizeof(int);
			}
	};

} /* unnamed namespace */


/**
 * Utility: Get server socket for given thread
 */
static int lx_server_socket(Genode::Thread_base *thread)
{
	int sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sd < 0)
		return sd;

	/*
	 * Main thread uses 'Ipc_server' for 'sleep_forever()' only. No need
	 * for binding.
	 */
	if (!thread)
		return sd;

	sockaddr_un addr;
	lx_create_server_addr(&addr, thread->tid().tid);

	/* make sure bind succeeds */
	lx_unlink(addr.sun_path);

	int const bind_ret = lx_bind(sd, (sockaddr *)&addr, sizeof(addr));
	if (bind_ret < 0)
		return bind_ret;

	return sd;
}


/**
 * Utility: Send request to server and wait for reply
 */
static void lx_call(long thread_id,
                    Genode::Msgbuf_base &send_msgbuf,
                    Genode::Msgbuf_base &recv_msgbuf)
{
	int ret;

	Message send_msg(thread_id);

	/* create reply channel */
	enum { LOCAL_SOCKET  = 0, REMOTE_SOCKET = 1 };
	int reply_channel[2];

	ret = lx_socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, reply_channel);
	if (ret < 0) {
		PRAW("lx_socketpair failed with %d", ret);
		/* XXX */ wait_for_continue();
		throw Genode::Ipc_error();
	}

	/* assemble message */

	/* marshal reply capability */
	send_msg.marshal_socket(reply_channel[REMOTE_SOCKET]);

	/* marshal capabilities contained in 'send_msgbuf' */
	if (send_msgbuf.used_caps() > 0)
		PRAW("lx_call: marshal %d caps:", send_msgbuf.used_caps());

	for (unsigned i = 0; i < send_msgbuf.used_caps(); i++) {
		PRAW("         sd[%d]: %d", i, send_msgbuf.cap(i));
		send_msg.marshal_socket(lx_dup(send_msgbuf.cap(i)));
	}

	send_msg.buffer(send_msgbuf.buf, send_msgbuf.used_size());

	ret = lx_sendmsg(reply_channel[LOCAL_SOCKET], send_msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_sendmsg failed with %d in lx_call()", ret);
		while (1);
		/* XXX */ wait_for_continue();
		throw Genode::Ipc_error();
	}

	Message recv_msg;
	recv_msg.accept_sockets(Message::MAX_SDS_PER_MSG);

	recv_msg.buffer(recv_msgbuf.buf, recv_msgbuf.size());

	ret = lx_recvmsg(reply_channel[LOCAL_SOCKET], recv_msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_recvmsg failed with %d in lx_call()", ret);
		/* XXX */ wait_for_continue();
		throw Genode::Ipc_error();
	}

	/*
	 * 'lx_recvmsg()' returns the number of bytes received. remember this value
	 * in 'Genode::Msgbuf_base'
	 *
	 * XXX revisit whether we really need this information
	 */
	recv_msgbuf.used_size(ret);

	if (recv_msg.num_sockets() > 0) {
		PRAW("lx_call: got %d sockets in reply", recv_msg.num_sockets());
		for (unsigned i = 0; i < recv_msg.num_sockets(); i++) {
			PRAW("         sd[%d]: %d", i, recv_msg.socket_at_index(i));
			lx_close(recv_msg.socket_at_index(i));
		}
	}

	/* destroy reply channel */
	lx_close(reply_channel[LOCAL_SOCKET]);
	lx_close(reply_channel[REMOTE_SOCKET]);
}


/**
 * Utility: Wait for request from client
 *
 * \return  socket descriptor of reply capability
 */
static int lx_wait(Genode::Native_connection_state &cs,
                   Genode::Msgbuf_base &recv_msgbuf)
{
	Message msg;

	msg.accept_sockets(Message::MAX_SDS_PER_MSG);
	msg.buffer(recv_msgbuf.buf, recv_msgbuf.size());

	int ret = lx_recvmsg(cs, msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_recvmsg failed with %d in lx_wait()", ret);
		/* XXX */ wait_for_continue();
		throw Genode::Ipc_error();
	}

	/*
	 * 'lx_recvmsg()' returned message size, keep it in 'recv_msgbuf'.
	 *
	 * XXX revisit whether this information is actually needed.
	 */
	recv_msgbuf.used_size(ret);


	if (msg.num_sockets() > 1) {
		PRAW("lx_wait: got %d sockets from wait", msg.num_sockets());
		for (unsigned i = 0; i < msg.num_sockets(); i++) {
			PRAW("         sd[%d]: %d", i, msg.socket_at_index(i));

			/* don't close reply channel */
			if (i > 0)
				lx_close(msg.socket_at_index(i));
		}
	}

	int reply_socket = msg.unmarshal_socket();

	/*
	 * Copy-out additional sds from msg to recv_msgbuf
	 */

	return reply_socket;
}


/**
 * Utility: Send reply to client
 */
static void lx_reply(int reply_socket,
                     Genode::Msgbuf_base &send_msgbuf)
{
	Message msg;

	msg.buffer(send_msgbuf.buf, send_msgbuf.used_size());

	int ret = lx_sendmsg(reply_socket, msg.msg(), 0);
	if (ret < 0)
		PRAW("lx_sendmsg failed with %d in lx_reply()", ret);

	lx_close(reply_socket);
}

#endif /* _PLATFORM__LINUX_SOCKET_H_ */
