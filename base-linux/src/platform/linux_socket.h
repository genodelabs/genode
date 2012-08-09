/*
 * \brief  Linux socket utilities
 * \author Christian Helmuth
 * \author Norman Feske
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

/* Linux includes */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>

/* Genode bindings to Linux kernel */
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


/******************************
 ** File-descriptor registry **
 ******************************/

/*
 * We use the name of the Unix-domain socket as key to uniquely identify
 * entrypoints. When receiving a socket descriptor as IPC payload, we first
 * lookup the corresponding entrypoint ID. If we already possess a socket
 * descriptor pointing to the same entrypoint, we close the received one and
 * use the already known descriptor instead.
 */

namespace Genode
{
	template <unsigned MAX_FDS>
	class Socket_descriptor_registry;

	typedef Socket_descriptor_registry<100> Ep_socket_descriptor_registry;

	/**
	 * Return singleton instance of registry for tracking entrypoint sockets
	 */
	Ep_socket_descriptor_registry *ep_sd_registry();
}


template <unsigned MAX_FDS>
class Genode::Socket_descriptor_registry
{
	public:

		class Limit_reached { };
		class Aliased_global_id { };

	private:

		struct Entry
		{
			int fd;
			int global_id;

			/**
			 * Default constructor creates empty entry
			 */
			Entry() : fd(-1), global_id(-1) { }

			Entry(int fd, int global_id) : fd(fd), global_id(global_id) { }

			bool is_free() const { return fd == -1; }
		};

		Entry _entries[MAX_FDS];

		Genode::Lock mutable _lock;

		Entry &_find_free_entry()
		{
			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].is_free())
					return _entries[i];

			throw Limit_reached();
		}

		bool _is_registered(int global_id) const
		{
			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].global_id == global_id)
					return true;

			return false;
		}

	public:

		/**
		 * Register association of socket descriptor and its corresponding ID
		 *
		 * \throw Limit_reached
		 * \throw Aliased_global_id  if global ID is already registered
		 */
		void associate(int sd, int global_id)
		{
			Genode::Lock::Guard guard(_lock);

			/* ignore invalid capabilities */
			if (sd == -1 || global_id == -1)
				return;

			/*
			 * Check for potential aliasing
			 *
			 * We allow any global ID to be present in the registry only once.
			 */
			if (_is_registered(global_id)) {
				PRAW("attempted to register global ID %d twice", global_id);
				throw Aliased_global_id();
			}

			Entry &entry = _find_free_entry();
			entry = Entry(sd, global_id);
		}

		/**
		 * Lookup file descriptor that belongs to specified global ID
		 *
		 * \return file descriptor or -1 if lookup failed
		 */
		int lookup_fd_by_global_id(int global_id) const
		{
			Genode::Lock::Guard guard(_lock);

			for (unsigned i = 0; i < MAX_FDS; i++)
				if (_entries[i].global_id == global_id)
					return _entries[i].fd;

			return -1;
		}
};


/**
 * Utility: Create socket address for server entrypoint at thread ID
 */
struct Uds_addr : sockaddr_un
{
	Uds_addr(long thread_id)
	{
		sun_family = AF_UNIX;
		Genode::snprintf(sun_path, sizeof(sun_path), "%s/ep-%ld",
		                 lx_rpath(), thread_id);
	}
};



/**
 * Utility: Return thread ID to which the given socket is directed to
 *
 * \return -1  if the socket is pointing to a valid entrypoint
 */
static int lookup_tid_by_client_socket(int sd)
{
	sockaddr_un name;
	socklen_t name_len = sizeof(name);
	int ret = lx_getpeername(sd, (sockaddr *)&name, &name_len);
	if (ret < 0)
		return -1;

	/*
	 * The socket name has the form '<rpath>/ep-<tid>'. Hence, to determine the
	 * tid, we have to skip 'strlen(rpath)' and 'strlen("/ep-"). We use static
	 * variables to determine length of 'rpath' only once because it is not
	 * going to change at runtime.
	 */
	static size_t const rpath_len = Genode::strlen(lx_rpath());
	static size_t const ep_suffix = Genode::strlen("/ep-");

	/*
	 * Reply capabilities do not have a name because they are created via
	 * 'socketpair'. Check if this function is called with such a socket
	 * descriptor as argument.
	 */
	if (rpath_len + ep_suffix >= name_len) {
		PRAW("Error: unexpectedly short socket name");
		return -1;
	}

	unsigned tid = 0;
	if (Genode::ascii_to(name.sun_path + rpath_len + ep_suffix, &tid) == 0) {
		PRAW("Error: could not parse tid number");
		return -1;
	}
	return tid;
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

			Message(void *buffer, size_t buffer_len) : _num_sds(0)
			{
				memset(&_msg, 0, sizeof(_msg));

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

				/* initialize iovec */
				_msg.msg_iov    = &_iovec;
				_msg.msg_iovlen = 1;

				_iovec.iov_base = buffer;
				_iovec.iov_len  = buffer_len;
			}

			msghdr * msg() { return &_msg; }

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

			int socket_at_index(int index) const
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
 * Utility: Extract socket desriptors from SCM message into 'Genode::Msgbuf'
 */
static void extract_sds_from_message(unsigned start_index, Message const &msg,
                                     Genode::Msgbuf_base &buf)
{
	buf.reset_caps();

	/* start at offset 1 to skip the reply channel */
	for (unsigned i = start_index; i < msg.num_sockets(); i++) {

		int const sd = msg.socket_at_index(i);
		int const id = lookup_tid_by_client_socket(sd);
		int const existing_sd = Genode::ep_sd_registry()->lookup_fd_by_global_id(id);

		if (existing_sd >= 0) {
			lx_close(sd);
			buf.append_cap(existing_sd);
		} else {
			Genode::ep_sd_registry()->associate(sd, id);
			buf.append_cap(sd);
		}
	}
}


class Server_socket_failed { };
class Client_socket_failed { };
class Bind_failed          { };
class Connect_failed       { };


/**
 * Utility: Create named socket pair for given thread
 *
 * \throw Server_socket_failed
 * \throw Client_socket_failed
 * \throw Bind_failed
 * \throw Connect_failed
 */
static inline Genode::Native_connection_state lx_server_socket_pair(long thread_id)
{
	Genode::Native_connection_state ncs;

	/*
	 * Main thread uses 'Ipc_server' for 'sleep_forever()' only. No need for
	 * binding.
	 */
	if (thread_id == -1)
		return ncs;

	Uds_addr addr(thread_id);

	/*
	 * Create server-side socket
	 */
	ncs.server_sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (ncs.server_sd < 0) {
		PRAW("Error: Could not create server-side socket (ret=%d)", ncs.server_sd);
		throw Server_socket_failed();
	}

	/* make sure bind succeeds */
	lx_unlink(addr.sun_path);

	int const bind_ret = lx_bind(ncs.server_sd, (sockaddr *)&addr, sizeof(addr));
	if (bind_ret < 0) {
		PRAW("Error: Could not bind server socket (ret=%d)", bind_ret);
		throw Bind_failed();
	}

	/*
	 * Create client-side socket
	 */
	ncs.client_sd = lx_socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (ncs.client_sd < 0) {
		PRAW("Error: Could not create client-side socket (ret=%d)", ncs.client_sd);
		throw Client_socket_failed();
	}

	int const conn_ret = lx_connect(ncs.client_sd, (sockaddr *)&addr, sizeof(addr));
	if (conn_ret < 0) {
		PRAW("Error: Could not connect client-side socket (ret=%d)", conn_ret);
		throw Connect_failed();
	}

	int const tid = lookup_tid_by_client_socket(ncs.client_sd);
	Genode::ep_sd_registry()->associate(ncs.client_sd, tid);

	/*
	 * Wipe Unix domain socket from the file system. It will live as long as
	 * there exist references to it in the form of file descriptors.
	 */
	lx_unlink(addr.sun_path);

	return ncs;
}


/**
 * Utility: Send request to server and wait for reply
 */
static inline void lx_call(int dst_sd,
                           Genode::Msgbuf_base &send_msgbuf, size_t send_msg_len,
                           Genode::Msgbuf_base &recv_msgbuf)
{
	int ret;
	Message send_msg(send_msgbuf.buf, send_msg_len);

	/* create reply channel */
	enum { LOCAL_SOCKET  = 0, REMOTE_SOCKET = 1 };
	int reply_channel[2];

	ret = lx_socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, reply_channel);
	if (ret < 0) {
		PRAW("lx_socketpair failed with %d", ret);
		throw Genode::Ipc_error();
	}

	/* assemble message */

	/* marshal reply capability */
	send_msg.marshal_socket(reply_channel[REMOTE_SOCKET]);

	/* marshal capabilities contained in 'send_msgbuf' */
	for (unsigned i = 0; i < send_msgbuf.used_caps(); i++)
		send_msg.marshal_socket(send_msgbuf.cap(i));

	ret = lx_sendmsg(dst_sd, send_msg.msg(), 0);
	if (ret < 0) {
		PRAW("[%d] lx_sendmsg to sd %d failed with %d in lx_call()",
		     lx_getpid(), dst_sd, ret);
		throw Genode::Ipc_error();
	}

	/* receive reply */

	Message recv_msg(recv_msgbuf.buf, recv_msgbuf.size());
	recv_msg.accept_sockets(Message::MAX_SDS_PER_MSG);

	ret = lx_recvmsg(reply_channel[LOCAL_SOCKET], recv_msg.msg(), 0);
	if (ret < 0) {
		PRAW("[%d] lx_recvmsg failed with %d in lx_call()", lx_getpid(), ret);
		throw Genode::Ipc_error();
	}

	extract_sds_from_message(0, recv_msg, recv_msgbuf);

	/* destroy reply channel */
	lx_close(reply_channel[LOCAL_SOCKET]);
	lx_close(reply_channel[REMOTE_SOCKET]);
}


/**
 * Utility: Wait for request from client
 *
 * \return  socket descriptor of reply capability
 */
static inline int lx_wait(Genode::Native_connection_state &cs,
                          Genode::Msgbuf_base &recv_msgbuf)
{
	Message msg(recv_msgbuf.buf, recv_msgbuf.size());

	msg.accept_sockets(Message::MAX_SDS_PER_MSG);

	int ret = lx_recvmsg(cs.server_sd, msg.msg(), 0);
	if (ret < 0) {
		PRAW("lx_recvmsg failed with %d in lx_wait(), sd=%d", ret, cs.server_sd);
		throw Genode::Ipc_error();
	}

	int const reply_socket = msg.socket_at_index(0);

	extract_sds_from_message(1, msg, recv_msgbuf);

	return reply_socket;
}


/**
 * Utility: Send reply to client
 */
static inline void lx_reply(int reply_socket,
                            Genode::Msgbuf_base &send_msgbuf,
                            size_t msg_len)
{
	Message msg(send_msgbuf.buf, msg_len);

	/*
	 * Marshall capabilities to be transferred to the client
	 */
	for (unsigned i = 0; i < send_msgbuf.used_caps(); i++)
		msg.marshal_socket(send_msgbuf.cap(i));

	int ret = lx_sendmsg(reply_socket, msg.msg(), 0);
	if (ret < 0)
		PRAW("lx_sendmsg failed with %d in lx_reply()", ret);

	lx_close(reply_socket);
}

#endif /* _PLATFORM__LINUX_SOCKET_H_ */
