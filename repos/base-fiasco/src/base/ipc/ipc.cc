/*
 * \brief  IPC implementation for Fiasco
 * \author Norman Feske
 * \date   2006-06-13
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/ipc.h>
#include <base/blocking.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
}

using namespace Genode;


class Msg_header
{
	private:

		/* kernel-defined message header */
		Fiasco::l4_fpage_t   rcv_fpage; /* unused */
		Fiasco::l4_msgdope_t size_dope;
		Fiasco::l4_msgdope_t send_dope;

	public:

		/*
		 * First two data words of message, used to transfer the local name of
		 * the invoked object (when a client calls a server) or the exception
		 * code (when the server replies), and the number of capability
		 * arguments. The kernel does not fetch these data words from memory
		 * but transfers them via the short-IPC registers.
		 */
		Fiasco::l4_umword_t protocol_word;
		Fiasco::l4_umword_t num_caps;

	private:

		enum { MAX_CAPS_PER_MSG = Msgbuf_base::MAX_CAPS_PER_MSG };

		Fiasco::l4_threadid_t _cap_tid        [MAX_CAPS_PER_MSG];
		unsigned long         _cap_local_name [MAX_CAPS_PER_MSG];

		size_t _num_msg_words(size_t num_data_words) const
		{
			size_t const caps_size = sizeof(_cap_tid) + sizeof(_cap_local_name);

			/*
			 * Account for the transfer of the protocol word, capability count,
			 * and capability arguments in front of the payload.
			 */
			return 2 + caps_size/sizeof(Fiasco::l4_umword_t) + num_data_words;
		}

	public:

		void *msg_start() { return &rcv_fpage; }

		/**
		 * Load header fields according to send-message buffer
		 */
		void prepare_snd_msg(unsigned long protocol, Msgbuf_base const &snd_msg)
		{
			using namespace Fiasco;

			protocol_word = protocol;
			num_caps = min((unsigned)MAX_CAPS_PER_MSG, snd_msg.used_caps());

			size_t const snd_words = snd_msg.data_size()/sizeof(l4_umword_t);
			send_dope = L4_IPC_DOPE(_num_msg_words(snd_words), 0);

			/* reset _cap_tid and _cap_local_name */
			for (unsigned i = 0; i < MAX_CAPS_PER_MSG; i++) {
				_cap_tid[i]        = L4_INVALID_ID;
				_cap_local_name[i] = 0;
			}

			for (unsigned i = 0; i < num_caps; i++) {
				Native_capability const &cap = snd_msg.cap(i);
				_cap_tid[i]        = cap.dst();
				_cap_local_name[i] = cap.local_name();
			}
		}

		/**
		 * Prepare header for receiving a message
		 */
		void prepare_rcv_msg(Msgbuf_base const &rcv_msg)
		{
			using namespace Fiasco;

			size_t const rcv_max_words = rcv_msg.capacity()/sizeof(l4_umword_t);

			size_dope = L4_IPC_DOPE(_num_msg_words(rcv_max_words), 0);
		}

		/**
		 * Copy received capability arguments into receive message buffer
		 */
		void extract_caps(Msgbuf_base &rcv_msg) const
		{
			for (unsigned i = 0; i < min((unsigned)MAX_CAPS_PER_MSG, num_caps); i++)
				rcv_msg.insert(Native_capability(_cap_tid[i],
				                                 _cap_local_name[i]));
		}
};


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t)
{
	using namespace Fiasco;

	Msg_header &snd_header = snd_msg.header<Msg_header>();
	snd_header.prepare_snd_msg(dst.local_name(), snd_msg);

	Msg_header &rcv_header = rcv_msg.header<Msg_header>();
	rcv_header.prepare_rcv_msg(rcv_msg);

	l4_msgdope_t ipc_result;
	l4_ipc_call(dst.dst(),
	            snd_header.msg_start(),
	            snd_header.protocol_word,
	            snd_header.num_caps,
	            rcv_header.msg_start(),
	            &rcv_header.protocol_word,
	            &rcv_header.num_caps,
	            L4_IPC_NEVER, &ipc_result);

	rcv_header.extract_caps(rcv_msg);

	if (L4_IPC_IS_ERROR(ipc_result)) {

		if (L4_IPC_ERROR(ipc_result) == L4_IPC_RECANCELED)
			throw Genode::Blocking_canceled();

		PERR("ipc_call error %lx", L4_IPC_ERROR(ipc_result));
		throw Genode::Ipc_error();
	}

	return Rpc_exception_code(rcv_header.protocol_word);
}


/****************
 ** IPC server **
 ****************/

void Genode::ipc_reply(Native_capability caller, Rpc_exception_code exc,
                       Msgbuf_base &snd_msg)
{
	using namespace Fiasco;

	Msg_header &snd_header = snd_msg.header<Msg_header>();
	snd_header.prepare_snd_msg(exc.value, snd_msg);

	l4_msgdope_t result;
	l4_ipc_send(caller.dst(), snd_header.msg_start(),
	            snd_header.protocol_word,
	            snd_header.num_caps,
	            L4_IPC_SEND_TIMEOUT_0, &result);

	if (L4_IPC_IS_ERROR(result))
		PERR("ipc_send error %lx, ignored", L4_IPC_ERROR(result));
}


Genode::Rpc_request Genode::ipc_reply_wait(Reply_capability const &last_caller,
                                           Rpc_exception_code      exc,
                                           Msgbuf_base            &reply_msg,
                                           Msgbuf_base            &request_msg)
{
	using namespace Fiasco;

	l4_msgdope_t ipc_result;

	bool need_to_wait = true;

	Msg_header &snd_header = reply_msg.header<Msg_header>();
	snd_header.prepare_snd_msg(exc.value, reply_msg);

	request_msg.reset();
	Msg_header &rcv_header = request_msg.header<Msg_header>();
	rcv_header.prepare_rcv_msg(request_msg);

	l4_threadid_t caller = L4_INVALID_ID;

	if (last_caller.valid()) {

		/*
		 * Use short IPC for reply if possible. This is the common case of
		 * returning an integer as RPC result.
		 */
		l4_ipc_reply_and_wait(last_caller.dst(), snd_header.msg_start(),
		                      snd_header.protocol_word,
		                      snd_header.num_caps,
		                      &caller, rcv_header.msg_start(),
		                      &rcv_header.protocol_word,
		                      &rcv_header.num_caps,
		                      L4_IPC_SEND_TIMEOUT_0, &ipc_result);

		/*
		 * The error condition could be a message cut (which we want to ignore
		 * on the server side) or a reply failure (for example, if the caller
		 * went dead during the call. In both cases, we do not reflect the
		 * error condition to the user but want to wait for the next proper
		 * incoming message.
		 */
		if (L4_IPC_IS_ERROR(ipc_result)) {
			PERR("ipc_reply_and_wait error %lx", L4_IPC_ERROR(ipc_result));
		} else {
			need_to_wait = false;
		}
	}

	while (need_to_wait) {

		l4_ipc_wait(&caller, rcv_header.msg_start(),
		            &rcv_header.protocol_word,
		            &rcv_header.num_caps,
		            L4_IPC_NEVER, &ipc_result);

		if (L4_IPC_IS_ERROR(ipc_result)) {
			PERR("ipc_wait error %lx", L4_IPC_ERROR(ipc_result));
		} else {
			need_to_wait = false;
		}
	};

	rcv_header.extract_caps(request_msg);

	return Rpc_request(Native_capability(caller, 0), rcv_header.protocol_word);
}


Ipc_server::Ipc_server() : Native_capability(Fiasco::l4_myself(), 0) { }


Ipc_server::~Ipc_server() { }
