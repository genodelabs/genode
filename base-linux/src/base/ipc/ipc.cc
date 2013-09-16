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
 * Copyright (C) 2011-2013 Genode Labs GmbH
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

/* local includes */
#include <socket_descriptor_registry.h>

/* Linux includes */
#include <linux_syscalls.h>
#include <sys/un.h>
#include <sys/socket.h>


using namespace Genode;


/*****************************
 ** IPC marshalling support **
 *****************************/

void Ipc_ostream::_marshal_capability(Native_capability const &cap)
{
	if (cap.valid()) {
		_write_to_buf(cap.local_name());

		_snd_msg->append_cap(cap.dst().socket);
	} else {
		_write_to_buf(-1L);
	}
}


void Ipc_istream::_unmarshal_capability(Native_capability &cap)
{
	long local_name =  0;
	_read_from_buf(local_name);

	if (local_name == -1) {

		/* construct invalid capability */
		cap = Genode::Native_capability();

	} else {

		/* construct valid capability */
		int const socket = _rcv_msg->read_cap();
		cap = Native_capability(Cap_dst_policy::Dst(socket), local_name);
	}
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

	/*
	 * Helper to destroy the server socket pair
	 *
	 * For core, this is a no-op. For all other processes, the server and client
	 * sockets are closed.
	 */
	void destroy_server_socket_pair(Native_connection_state const &ncs);
}


/******************************
 ** File-descriptor registry **
 ******************************/

Genode::Ep_socket_descriptor_registry *Genode::ep_sd_registry()
{
	static Genode::Ep_socket_descriptor_registry registry;
	return &registry;
}


/********************************************
 ** Communication over Unix-domain sockets **
 ********************************************/

enum {
	LX_EINTR        = 4,
	LX_ECONNREFUSED = 111
};


/**
 * Utility: Return thread ID to which the given socket is directed to
 *
 * \return -1  if the socket is pointing to a valid entrypoint
 */
static int lookup_tid_by_client_socket(int sd)
{
	/*
	 * Synchronize calls so that the large 'sockaddr_un' can be allocated
	 * in the BSS rather than the stack.
	 */
	Lock lock;
	Lock::Guard guard(lock);

	static sockaddr_un name;
	socklen_t name_len = sizeof(name);
	int ret = lx_getpeername(sd, (sockaddr *)&name, &name_len);
	if (ret < 0)
		return -1;

	struct Prefix_len
	{
		typedef Genode::size_t size_t;

		size_t const len;

		static int _init_len(char const *s)
		{
			char  const * const pattern     = "/ep-";
			static size_t const pattern_len = Genode::strlen(pattern);

			for (size_t i = 0; Genode::strlen(s + i) >= pattern_len; i++)
				if (Genode::strcmp(s + i, pattern, pattern_len) == 0)
					return i + pattern_len;

			struct Unexpected_rpath_prefix { };
			throw  Unexpected_rpath_prefix();
		}

		Prefix_len(char const *s) : len(_init_len(s)) { }
	};

	/*
	 * The name of the Unix-domain socket has the form <rpath>-<uid>/ep-<tid>.
	 * We are only interested in the <tid> part. Hence, we determine the length
	 * of the <rpath>-<uid>/ep- portion only once and keep it in a static
	 * variable.
	 */
	static Prefix_len prefix_len(name.sun_path);

	unsigned tid = 0;
	if (Genode::ascii_to(name.sun_path + prefix_len.len, &tid) == 0) {
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

			typedef Genode::size_t size_t;

			msghdr      _msg;
			sockaddr_un _addr;
			iovec       _iovec;
			char        _cmsg_buf[CMSG_SPACE(MAX_SDS_PER_MSG*sizeof(int))];

			unsigned _num_sds;

		public:

			Message(void *buffer, size_t buffer_len) : _num_sds(0)
			{
				Genode::memset(&_msg, 0, sizeof(_msg));

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

		int const associated_sd = Genode::ep_sd_registry()->try_associate(sd, id);

		buf.append_cap(associated_sd);

		if ((associated_sd >= 0) && (associated_sd != sd)) {

			/*
			 * The association already existed under a different name, use
			 * already associated socket descriptor and and drop 'sd'.
			 */
			lx_close(sd);
		}
	}
}


/**
 * Send request to server and wait for reply
 */
static inline void lx_call(int dst_sd,
                           Genode::Msgbuf_base &send_msgbuf, Genode::size_t send_msg_len,
                           Genode::Msgbuf_base &recv_msgbuf)
{
	int ret;
	Message send_msg(send_msgbuf.buf, send_msg_len);

	/*
	 * Create reply channel
	 *
	 * The reply channel will be closed when leaving the scope of 'lx_call'.
	 */
	struct Reply_channel
	{
		enum { LOCAL_SOCKET = 0, REMOTE_SOCKET = 1 };
		int sd[2];

		Reply_channel()
		{
			sd[LOCAL_SOCKET] = -1; sd[REMOTE_SOCKET] = -1;

			int ret = lx_socketpair(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, sd);
			if (ret < 0) {
				PRAW("[%d] lx_socketpair failed with %d", lx_getpid(), ret);
				throw Genode::Ipc_error();
			}
		}

		~Reply_channel()
		{
			if (sd[LOCAL_SOCKET]  != -1) lx_close(sd[LOCAL_SOCKET]);
			if (sd[REMOTE_SOCKET] != -1) lx_close(sd[REMOTE_SOCKET]);
		}

		int local_socket()  const { return sd[LOCAL_SOCKET];  }
		int remote_socket() const { return sd[REMOTE_SOCKET]; }

	} reply_channel;

	/* assemble message */

	/* marshal reply capability */
	send_msg.marshal_socket(reply_channel.remote_socket());

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

	ret = lx_recvmsg(reply_channel.local_socket(), recv_msg.msg(), 0);

	/* system call got interrupted by a signal */
	if (ret == -LX_EINTR)
		throw Genode::Blocking_canceled();

	if (ret < 0) {
		PRAW("[%d] lx_recvmsg failed with %d in lx_call()", lx_getpid(), ret);
		throw Genode::Ipc_error();
	}

	extract_sds_from_message(0, recv_msg, recv_msgbuf);
}


/**
 * for request from client
 *
 * \return  socket descriptor of reply capability
 */
static inline int lx_wait(Genode::Native_connection_state &cs,
                          Genode::Msgbuf_base &recv_msgbuf)
{
	Message msg(recv_msgbuf.buf, recv_msgbuf.size());

	msg.accept_sockets(Message::MAX_SDS_PER_MSG);

	int ret = lx_recvmsg(cs.server_sd, msg.msg(), 0);

	/* system call got interrupted by a signal */
	if (ret == -LX_EINTR)
		throw Genode::Blocking_canceled();

	if (ret < 0) {
		PRAW("lx_recvmsg failed with %d in lx_wait(), sd=%d", ret, cs.server_sd);
		throw Genode::Ipc_error();
	}

	int const reply_socket = msg.socket_at_index(0);

	extract_sds_from_message(1, msg, recv_msgbuf);

	return reply_socket;
}


/**
 * Send reply to client
 */
static inline void lx_reply(int reply_socket,
                            Genode::Msgbuf_base &send_msgbuf,
                            Genode::size_t msg_len)
{
	Message msg(send_msgbuf.buf, msg_len);

	/*
	 * Marshall capabilities to be transferred to the client
	 */
	for (unsigned i = 0; i < send_msgbuf.used_caps(); i++)
		msg.marshal_socket(send_msgbuf.cap(i));

	int ret = lx_sendmsg(reply_socket, msg.msg(), 0);

	/* ignore reply send error caused by disappearing client */
	if (ret >= 0 || ret == -LX_ECONNREFUSED) {
		lx_close(reply_socket);
		return;
	}

	if (ret < 0)
		PRAW("[%d] lx_sendmsg failed with %d in lx_reply()", lx_getpid(), ret);
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


Ipc_istream::~Ipc_istream()
{
	/*
	 * The association of the capability (client) socket must be invalidated on
	 * server destruction. We implement it here as the IPC server currently has
	 * no destructor. We have the plan to remove Ipc_istream and Ipc_ostream
	 * in the future and, then, move this into the server destructor.
	 *
	 * IPC clients have -1 as client_sd and need no disassociation.
	 */
	if (_rcv_cs.client_sd != -1) {
		Genode::ep_sd_registry()->disassociate(_rcv_cs.client_sd);

		/*
		 * Reset thread role to non-server such that we can enter 'sleep_forever'
		 * without getting a warning.
		 */
		Thread_base *thread = Thread_base::myself();
		if (thread)
			thread->tid().is_ipc_server = false;
	}

	destroy_server_socket_pair(_rcv_cs);
	_rcv_cs.client_sd = -1;
	_rcv_cs.server_sd = -1;
}


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


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg, unsigned short)
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
		PRAW("[%d] unexpected multiple instantiation of Ipc_server by one thread",
		     lx_gettid());
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
