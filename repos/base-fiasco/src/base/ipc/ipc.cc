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


class Msg_header
{
	private:

		/* kernel-defined message header */
		Fiasco::l4_fpage_t   rcv_fpage; /* unused */
		Fiasco::l4_msgdope_t size_dope;
		Fiasco::l4_msgdope_t send_dope;

		/*
		 * First data word of message, used to transfer the local name of the
		 * invoked object (when a client calls a server) or the exception code
		 * (when the server replies). This data word is never fetched from
		 * memory but transferred via the first short-IPC register. The
		 * 'protocol_word' is needed as a spacer between the header fields
		 * define above and the regular message payload..
		 */
		Fiasco::l4_umword_t protocol_word;

	public:

		void *msg_start() { return &rcv_fpage; }

		/**
		 * Define message size for sending
		 */
		void snd_size(Genode::size_t size)
		{
			using namespace Fiasco;

			/* account for the transfer of the protocol word in front of the payload */
			Genode::size_t const snd_words = size/sizeof(l4_umword_t);
			send_dope = L4_IPC_DOPE(snd_words + 1, 0);
		}

		/**
		 * Define size of receive buffer
		 */
		void rcv_capacity(Genode::size_t capacity)
		{
			using namespace Fiasco;

			size_dope = L4_IPC_DOPE(capacity/sizeof(l4_umword_t), 0);
		}

		void *msg_type(Genode::size_t size)
		{
			using namespace Fiasco;

			return size <= sizeof(l4_umword_t) ? L4_IPC_SHORT_MSG : msg_start();
		}
};


using namespace Genode;


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t)
{
	using namespace Fiasco;

	Msg_header &snd_header = snd_msg.header<Msg_header>();
	snd_header.snd_size(snd_msg.data_size());

	Msg_header &rcv_header = rcv_msg.header<Msg_header>();
	rcv_header.rcv_capacity(rcv_msg.capacity());

	l4_msgdope_t ipc_result;
	l4_umword_t exception_code = 0;
	l4_ipc_call(dst.dst(),
	            snd_header.msg_type(snd_msg.data_size()),
	            dst.local_name(),
	            snd_msg.word(0),
	            rcv_header.msg_start(),
	            &exception_code,
	            &rcv_msg.word(0),
	            L4_IPC_NEVER, &ipc_result);

	if (L4_IPC_IS_ERROR(ipc_result)) {

		if (L4_IPC_ERROR(ipc_result) == L4_IPC_RECANCELED)
			throw Genode::Blocking_canceled();

		PERR("Ipc error %lx", L4_IPC_ERROR(ipc_result));
		throw Genode::Ipc_error();
	}

	return Rpc_exception_code(exception_code);
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	_reply_needed = true;
	_read_offset = _write_offset = 0;
}


static umword_t wait(Native_connection_state &rcv_cs, Msgbuf_base &rcv_msg)
{
	using namespace Fiasco;

	l4_msgdope_t result;
	umword_t badge = 0;

	/*
	 * Wait until we get a proper message and thereby
	 * ignore receive message cuts on the server-side.
	 * This error condition should be handled by the
	 * client. The server does not bother.
	 */
	do {
		Msg_header &rcv_header = rcv_msg.header<Msg_header>();
		rcv_header.rcv_capacity(rcv_msg.capacity());

		l4_ipc_wait(&rcv_cs.caller, rcv_header.msg_start(),
		            &badge,
		            &rcv_msg.word(0),
		            L4_IPC_NEVER, &result);

		if (L4_IPC_IS_ERROR(result))
			PERR("Ipc error %lx", L4_IPC_ERROR(result));

	} while (L4_IPC_IS_ERROR(result));

	return badge;
}


void Ipc_server::reply()
{
	using namespace Fiasco;

	Msg_header &snd_header = _snd_msg.header<Msg_header>();
	snd_header.snd_size(_snd_msg.data_size());

	l4_msgdope_t result;
	l4_ipc_send(_caller.dst(), snd_header.msg_start(),
	            _exception_code.value,
	            _snd_msg.word(0),
	            L4_IPC_SEND_TIMEOUT_0, &result);

	if (L4_IPC_IS_ERROR(result))
		PERR("Ipc error %lx, ignored", L4_IPC_ERROR(result));

	_prepare_next_reply_wait();
}


void Ipc_server::reply_wait()
{
	using namespace Fiasco;

	if (_reply_needed) {

		l4_msgdope_t ipc_result;

		Msg_header &snd_header = _snd_msg.header<Msg_header>();
		snd_header.snd_size(_snd_msg.data_size());

		Msg_header &rcv_header = _rcv_msg.header<Msg_header>();
		rcv_header.rcv_capacity(_rcv_msg.capacity());

		/*
		 * Use short IPC for reply if possible. This is the common case of
		 * returning an integer as RPC result.
		 */
		l4_ipc_reply_and_wait(
		            _caller.dst(),
		            snd_header.msg_type(_snd_msg.data_size()),
		            _exception_code.value,
		            _snd_msg.word(0),
		            &_rcv_cs.caller, rcv_header.msg_start(),
		            &_badge,
		            &_rcv_msg.word(0),
		            L4_IPC_SEND_TIMEOUT_0, &ipc_result);

		if (L4_IPC_IS_ERROR(ipc_result)) {
			PERR("Ipc error %lx", L4_IPC_ERROR(ipc_result));

			/*
			 * The error conditions could be a message cut (which
			 * we want to ignore on the server side) or a reply failure
			 * (for example, if the caller went dead during the call.
			 * In both cases, we do not reflect the error condition to
			 * the user but want to wait for the next proper incoming
			 * message. So let's just wait now.
			 */
			_badge = wait(_rcv_cs, _rcv_msg);
		}
	} else {
		_badge = wait(_rcv_cs, _rcv_msg);
	}

	/* define destination of next reply */
	_caller = Native_capability(_rcv_cs.caller, 0);

	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg),
	Native_capability(Fiasco::l4_myself(), 0),
	_rcv_cs(cs)
{ }


Ipc_server::~Ipc_server() { }
