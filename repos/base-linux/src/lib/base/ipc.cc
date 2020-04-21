/*
 * \brief  Socket-based IPC implementation for Linux
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Stefan Thoeni
 * \date   2011-10-11
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>
#include <base/blocking.h>
#include <base/env.h>
#include <base/sleep.h>
#include <linux_native_cpu/linux_native_cpu.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/ipc_server.h>
#include <base/internal/capability_space_tpl.h>

/* Linux includes */
#include <linux_syscalls.h>

using namespace Genode;

namespace {

	struct Pid
	{
		int value;

		Pid() : value(lx_getpid()) { }

		void print(Output &out) const { Genode::print(out, "[", value, "]"); }
	};
}


/*
 * The request message layout is:
 *
 *   long  local_name;
 *   ...call arguments, starting with the opcode...
 *
 * Response messages look like this:
 *
 *   long  exception code
 *   ...call results...
 *
 * First data word of message, used to transfer the exception code (when the
 * server replies). This data word is never fetched from memory but
 * transferred via the first short-IPC register. The 'protocol_word' is needed
 * as a spacer between the header fields defined above and the regular message
 * payload.
 */
struct Protocol_header
{
	/* badge of invoked object (on call) / exception code (on reply) */
	unsigned long protocol_word;

	Genode::size_t num_caps;

	/* badges of the transferred capability arguments */
	unsigned long badges[Msgbuf_base::MAX_CAPS_PER_MSG];

	enum { INVALID_BADGE = ~1UL };

	void *msg_start() { return &protocol_word; }
};


/*
 * The INVALID_BADGE must be different from the representation of an
 * invalid RPC object key because this key value is used by manually
 * created NON-RPC-object capabilities (client_sd, server_sd, dataspace fd).
 */
static_assert((int)Protocol_header::INVALID_BADGE != (int)Rpc_obj_key::INVALID,
              "ambigious INVALID_BADGE");


/********************************************
 ** Communication over Unix-domain sockets **
 ********************************************/

enum {
	LX_EINTR        = 4,
	LX_EAGAIN       = 11,
	LX_ECONNREFUSED = 111
};


namespace {

	/**
	 * Message object encapsulating data for sendmsg/recvmsg
	 */
	struct Message
	{
		public:

			enum { MAX_SDS_PER_MSG = Genode::Msgbuf_base::MAX_CAPS_PER_MSG + 1 };

		private:

			typedef Genode::size_t size_t;

			msghdr      _msg   { };
			sockaddr_un _addr  { };
			iovec       _iovec { };
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

			void marshal_socket(Lx_sd sd)
			{
				*((int *)CMSG_DATA((cmsghdr *)_cmsg_buf) + _num_sds) = sd.value;

				struct cmsghdr *cmsg = CMSG_FIRSTHDR(&_msg);
				if (cmsg) {
					_num_sds++;
					cmsg->cmsg_len       = CMSG_LEN(_num_sds*sizeof(int));
					_msg.msg_controllen  = cmsg->cmsg_len;     /* actual cmsg length */
				}
			}

			void accept_sockets(int num_sds)
			{
				struct cmsghdr *cmsg = CMSG_FIRSTHDR(&_msg);
				cmsg->cmsg_len       = CMSG_LEN(num_sds*sizeof(int));
				_msg.msg_controllen  = cmsg->cmsg_len;     /* actual cmsg length */
			}

			Lx_sd socket_at_index(int index) const
			{
				return Lx_sd { *((int *)CMSG_DATA((cmsghdr *)_cmsg_buf) + index) };
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


static void insert_sds_into_message(Message &msg,
                                    Protocol_header &header,
                                    Genode::Msgbuf_base &snd_msgbuf)
{
	for (unsigned i = 0; i < snd_msgbuf.used_caps(); i++) {

		Native_capability const &cap = snd_msgbuf.cap(i);

		auto remote_socket_of_capability = [] (Native_capability const &cap)
		{
			if (!cap.valid())
				return Lx_sd::invalid();

			Capability_space::Ipc_cap_data cap_data =
				Capability_space::ipc_cap_data(cap);

			if (cap_data.dst.socket.value < 0)
				return Lx_sd::invalid();

			return cap_data.dst.socket;
		};

		Lx_sd const socket = remote_socket_of_capability(cap);

		if (socket.valid()) {
			msg.marshal_socket(socket);
			header.badges[i] = Capability_space::ipc_cap_data(cap).rpc_obj_key.value();
		} else {
			header.badges[i] = Protocol_header::INVALID_BADGE;
		}

	}
	header.num_caps = snd_msgbuf.used_caps();
}


/**
 * Utility: Extract socket desriptors from SCM message into 'Genode::Msgbuf'
 */
static void extract_sds_from_message(unsigned start_index,
                                     Message const &msg,
                                     Protocol_header const &header,
                                     Genode::Msgbuf_base &buf)
{
	unsigned sd_cnt = 0;
	for (unsigned i = 0; i < min(header.num_caps, (size_t)Msgbuf_base::MAX_CAPS_PER_MSG); i++) {

		unsigned long const badge = header.badges[i];

		/* an invalid capabity was transferred */
		if (badge == Protocol_header::INVALID_BADGE) {
			buf.insert(Native_capability());
			continue;
		}

		Lx_sd const sd = msg.socket_at_index(start_index + sd_cnt++);

		Rpc_destination const dst(sd);

		if (dst.valid())
			buf.insert(Capability_space::import(dst, Rpc_obj_key(badge)));
		else
			buf.insert(Native_capability());
	}
}


/**
 * Send reply to client
 */
static inline void lx_reply(Lx_sd reply_socket, Rpc_exception_code exception_code,
                            Genode::Msgbuf_base &snd_msgbuf)
{

	Protocol_header &header = snd_msgbuf.header<Protocol_header>();

	header.protocol_word = exception_code.value;

	Message msg(header.msg_start(), sizeof(Protocol_header) + snd_msgbuf.data_size());

	/* marshall capabilities to be transferred to the client */
	insert_sds_into_message(msg, header, snd_msgbuf);

	int const ret = lx_sendmsg(reply_socket, msg.msg(), 0);

	/* ignore reply send error caused by disappearing client */
	if (ret >= 0 || ret == -LX_ECONNREFUSED)
		return;

	if (ret < 0)
		error(lx_getpid(), ":", lx_gettid(), " lx_sendmsg failed with ", ret, " "
		    "in lx_reply() reply_socket=", reply_socket);
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msgbuf, Msgbuf_base &rcv_msgbuf,
                                    size_t)
{
	if (!dst.valid()) {
		error("attempt to call invalid capability, blocking forever");
		sleep_forever();
	}

	Protocol_header &snd_header = snd_msgbuf.header<Protocol_header>();
	snd_header.protocol_word = 0;

	Message snd_msg(snd_header.msg_start(),
	                sizeof(Protocol_header) + snd_msgbuf.data_size());

	/*
	 * Create reply channel
	 *
	 * The reply channel will be closed when leaving the scope of 'lx_call'.
	 */
	struct Reply_channel : Lx_socketpair
	{
		~Reply_channel()
		{
			if (local.value  != -1) lx_close(local.value);
			if (remote.value != -1) lx_close(remote.value);
		}
	} reply_channel;

	/* assemble message */

	/* marshal reply capability */
	snd_msg.marshal_socket(reply_channel.remote);

	/* marshal capabilities contained in 'snd_msgbuf' */
	insert_sds_into_message(snd_msg, snd_header, snd_msgbuf);

	Lx_sd const dst_socket = Capability_space::ipc_cap_data(dst).dst.socket;

	int const send_ret = lx_sendmsg(dst_socket, snd_msg.msg(), 0);
	if (send_ret < 0) {
		error(lx_getpid(), ":", lx_gettid(), " lx_sendmsg to sd ", dst_socket,
		    " failed with ", send_ret, " in lx_call()");
		sleep_forever();
	}

	/* receive reply */
	Protocol_header &rcv_header = rcv_msgbuf.header<Protocol_header>();
	rcv_header.protocol_word = 0;

	Message rcv_msg(rcv_header.msg_start(),
	                sizeof(Protocol_header) + rcv_msgbuf.capacity());
	rcv_msg.accept_sockets(Message::MAX_SDS_PER_MSG);

	rcv_msgbuf.reset();
	int const recv_ret = lx_recvmsg(reply_channel.local, rcv_msg.msg(), 0);

	/* system call got interrupted by a signal */
	if (recv_ret == -LX_EINTR)
		throw Genode::Blocking_canceled();

	if (recv_ret < 0) {
		error(lx_getpid(), ":", lx_gettid(), " ipc_call failed to receive result (", recv_ret, ")");
		sleep_forever();
	}

	extract_sds_from_message(0, rcv_msg, rcv_header, rcv_msgbuf);

	return Rpc_exception_code(rcv_header.protocol_word);
}


/****************
 ** IPC server **
 ****************/

void Genode::ipc_reply(Native_capability caller, Rpc_exception_code exc,
                       Msgbuf_base &snd_msg)
{
	Lx_sd const reply_socket = Capability_space::ipc_cap_data(caller).dst.socket;

	try { lx_reply(reply_socket, exc, snd_msg); } catch (Ipc_error) { }
}


Rpc_request Genode::ipc_reply_wait(Reply_capability const &last_caller,
                                   Rpc_exception_code      exc,
                                   Msgbuf_base            &reply_msg,
                                   Msgbuf_base            &request_msg)
{
	/* when first called, there was no request yet */
	if (last_caller.valid() && exc.value != Rpc_exception_code::INVALID_OBJECT)
		lx_reply(Capability_space::ipc_cap_data(last_caller).dst.socket, exc, reply_msg);

	/*
	 * Block infinitely if called from the main thread. This may happen if the
	 * main thread calls 'sleep_forever()'.
	 */
	Thread *myself_ptr = Thread::myself();
	if (!myself_ptr) {
		struct timespec ts = { 1000, 0 };
		for (;;) lx_nanosleep(&ts, 0);
	}

	Native_thread::Epoll &epoll = myself_ptr->native_thread().epoll;

	for (;;) {

		Lx_sd const selected_sd = epoll.poll();

		Protocol_header &header = request_msg.header<Protocol_header>();
		Message msg(header.msg_start(), sizeof(Protocol_header) + request_msg.capacity());

		msg.accept_sockets(Message::MAX_SDS_PER_MSG);

		request_msg.reset();
		int const ret = lx_recvmsg(selected_sd, msg.msg(), 0x40);

		if (ret < 0)
			continue;

		if (msg.num_sockets() == 0 || !msg.socket_at_index(0).valid()) {
			warning("ipc_reply_wait: failed to obtain reply socket");
			continue;
		}

		Lx_sd const reply_socket = msg.socket_at_index(0);

		/* start at offset 1 to skip the reply channel */
		extract_sds_from_message(1, msg, header, request_msg);

		return Rpc_request(Capability_space::import(Rpc_destination(reply_socket),
		                                            Rpc_obj_key()), selected_sd.value);
	}
}


Ipc_server::Ipc_server()
{
	/*
	 * If 'thread' is 0, the constructor was called by the main thread. By
	 * definition, main is never an RPC entrypoint. However, the main thread
	 * may call 'sleep_forever()', which instantiates 'Ipc_server'.
	 */
	if (!Thread::myself())
		return;

	Native_thread &native_thread = Thread::myself()->native_thread();

	if (native_thread.is_ipc_server) {
		Genode::raw(lx_getpid(), ":", lx_gettid(),
		            " unexpected multiple instantiation of Ipc_server by one thread");
		struct Ipc_server_multiple_instance { };
		throw Ipc_server_multiple_instance();
	}

	native_thread.is_ipc_server = true;
}


Ipc_server::~Ipc_server()
{
	if (!Thread::myself())
		return;

	/*
	 * Reset thread role to non-server such that we can enter 'sleep_forever'
	 * without getting a warning.
	 */
	Native_thread &native_thread = Thread::myself()->native_thread();

	native_thread.is_ipc_server = false;
}
