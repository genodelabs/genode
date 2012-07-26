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
		private:

			msghdr      _msg;
			sockaddr_un _addr;
			iovec       _iovec;
			char        _cmsg_buf[CMSG_SPACE(sizeof(int))];

		public:

			Message(long server_thread_id = -1)
			{
				memset(&_msg, 0, sizeof(_msg));

				if (server_thread_id != -1) {
					/* initialize receiver */
					lx_create_server_addr(&_addr, server_thread_id);

					_msg.msg_name    = &_addr;
					_msg.msg_namelen = sizeof(_addr);
				}
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

			/**
			 * Prepare slot for socket sending/reception
			 *
			 * Note, if this function is not called sockets are not accepted on
			 * 'recvmsg' and, therefore, do not occupy local file descriptors.
			 */
			void prepare_reply_socket_slot()
			{
				/* initialize control message */
				struct cmsghdr *cmsg;

				_msg.msg_control     = _cmsg_buf;
				_msg.msg_controllen  = sizeof(_cmsg_buf);  /* buffer space available */
				_msg.msg_flags      |= MSG_CMSG_CLOEXEC;
				cmsg                 = CMSG_FIRSTHDR(&_msg);
				cmsg->cmsg_len       = CMSG_LEN(sizeof(int));
				cmsg->cmsg_level     = SOL_SOCKET;
				cmsg->cmsg_type      = SCM_RIGHTS;
				_msg.msg_controllen  = cmsg->cmsg_len;     /* actual cmsg length */
			}

			void reply_socket(int sd)
			{
				if (!_msg.msg_control) {
					PRAW("reply-socket slot not prepared");
					throw Genode::Ipc_error();
				}

				*(int *)CMSG_DATA((cmsghdr *)_cmsg_buf) = sd;
			}

			int reply_socket() const
			{
				if (!_msg.msg_control) {
					PRAW("reply-socket slot not prepared");
					throw Genode::Ipc_error();
				}

				return *(int *)CMSG_DATA((cmsghdr *)_cmsg_buf);
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
                    void *send_buf, Genode::size_t send_buf_len,
                    void *recv_buf, Genode::size_t recv_buf_len)
{
	int ret;

	Message send_msg(thread_id);

	/* create reply channel */
	enum { LOCAL_SOCKET  = 0, REMOTE_SOCKET = 1 };
	int reply_channel[2];

	ret = lx_socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, reply_channel);
	if (ret < 0) {
		PRAW("lx_socketpair failed with %d", ret);
		throw Genode::Ipc_error();
	}

	send_msg.prepare_reply_socket_slot();
	send_msg.reply_socket(reply_channel[REMOTE_SOCKET]);
	send_msg.buffer(send_buf, send_buf_len);

	ret = lx_sendmsg(reply_channel[LOCAL_SOCKET], send_msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_sendmsg failed with %d in lx_call()", ret);
		throw Genode::Ipc_error();
	}

	Message recv_msg;

	recv_msg.buffer(recv_buf, recv_buf_len);

	ret = lx_recvmsg(reply_channel[LOCAL_SOCKET], recv_msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_recvmsg failed with %d in lx_call()", ret);
		throw Genode::Ipc_error();
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
                    void *buf, Genode::size_t buf_len)
{
	int ret;

	Message msg;

	msg.prepare_reply_socket_slot();
	msg.buffer(buf, buf_len);

	ret = lx_recvmsg(cs, msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_recvmsg failed with %d in lx_wait()", ret);
		throw Genode::Ipc_error();
	}

	return msg.reply_socket();
}


/**
 * Utility: Send reply to client
 */
static void lx_reply(int reply_socket,
                     void *buf, Genode::size_t buf_len)
{
	int ret;

	Message msg;

	msg.buffer(buf, buf_len);

	ret = lx_sendmsg(reply_socket, msg.msg(), 0);
	if (ret < 0)
		PRAW("lx_sendmsg failed with %d in lx_reply()", ret);

	lx_close(reply_socket);
}

#endif /* _PLATFORM__LINUX_SOCKET_H_ */
