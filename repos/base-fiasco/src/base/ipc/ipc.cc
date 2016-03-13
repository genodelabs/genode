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
#include <base/internal/native_connection_state.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
}

using namespace Genode;


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	using namespace Fiasco;

	l4_msgdope_t ipc_result;
	long         rec_badge;

	_snd_msg.send_dope = L4_IPC_DOPE((_write_offset + 2*sizeof(umword_t) - 1)>>2, 0);
	_rcv_msg.size_dope = L4_IPC_DOPE(_rcv_msg.size()>>2, 0);

	l4_ipc_call(_dst.dst(),
	            _write_offset <= 2*sizeof(umword_t) ? L4_IPC_SHORT_MSG : _snd_msg.msg_start(),
	            _dst.local_name(),
	            *reinterpret_cast<l4_umword_t *>(&_snd_msg.buf[sizeof(umword_t)]),
	            _rcv_msg.msg_start(),
	            reinterpret_cast<l4_umword_t *>(&rec_badge),
	            reinterpret_cast<l4_umword_t *>(&_rcv_msg.buf[sizeof(umword_t)]),
	            L4_IPC_NEVER, &ipc_result);

	if (L4_IPC_IS_ERROR(ipc_result)) {

		if (L4_IPC_ERROR(ipc_result) == L4_IPC_RECANCELED)
			throw Genode::Blocking_canceled();

		PERR("Ipc error %lx", L4_IPC_ERROR(ipc_result));
		throw Genode::Ipc_error();
	}

	/*
	 * Reset buffer read and write offsets. We shadow the first mword of the
	 * send message buffer (filled via '_write_offset') with the local name of
	 * the invoked remote object. We shadow the first mword of the receive
	 * buffer (retrieved via '_read_offset') with the local name of the reply
	 * capability ('rec_badge'), which is bogus in the L4/Fiasco case. In both
	 * cases, we skip the shadowed message mword when reading/writing the
	 * message payload.
	 */
	_write_offset = _read_offset = sizeof(umword_t);
}


Ipc_client::Ipc_client(Native_capability const &dst,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg, unsigned short)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg), _result(0), _dst(dst)
{
	_read_offset = _write_offset = sizeof(umword_t);
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	/* now we have a request to reply */
	_reply_needed = true;

	/* leave space for return value at the beginning of the msgbuf */
	_write_offset = 2*sizeof(umword_t);

	/* receive buffer offset */
	_read_offset = sizeof(umword_t);
}


void Ipc_server::wait()
{
	/* wait for new server request */
	try {

		using namespace Fiasco;

		l4_msgdope_t  result;

		/*
		 * Wait until we get a proper message and thereby
		 * ignore receive message cuts on the server-side.
		 * This error condition should be handled by the
		 * client. The server does not bother.
		 */
		do {
			_rcv_msg.size_dope = L4_IPC_DOPE(_rcv_msg.size()>>2, 0);

			l4_ipc_wait(&_rcv_cs.caller, _rcv_msg.msg_start(),
			            reinterpret_cast<l4_umword_t *>(&_rcv_msg.buf[0]),
			            reinterpret_cast<l4_umword_t *>(&_rcv_msg.buf[sizeof(umword_t)]),
			            L4_IPC_NEVER, &result);

			if (L4_IPC_IS_ERROR(result))
				PERR("Ipc error %lx", L4_IPC_ERROR(result));

		} while (L4_IPC_IS_ERROR(result));

		/* reset buffer read offset */
		_read_offset = sizeof(umword_t);
	
	} catch (Blocking_canceled) { }

	/* define destination of next reply */
	_caller = Native_capability(_rcv_cs.caller, 0);
	_badge  = reinterpret_cast<unsigned long *>(&_rcv_msg.buf)[0];

	_prepare_next_reply_wait();
}


void Ipc_server::reply()
{
	using namespace Fiasco;

	_snd_msg.send_dope = L4_IPC_DOPE((_write_offset + sizeof(umword_t) - 1)>>2, 0);

	l4_msgdope_t result;
	l4_ipc_send(_caller.dst(), _snd_msg.msg_start(),
	            _caller.local_name(),
	            *reinterpret_cast<l4_umword_t *>(&_snd_msg.buf[sizeof(umword_t)]),
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

		_snd_msg.send_dope = L4_IPC_DOPE((_write_offset + sizeof(umword_t) - 1)>>2, 0);
		_rcv_msg.size_dope = L4_IPC_DOPE(_rcv_msg.size()>>2, 0);

		/*
		 * Use short IPC for reply if possible.
		 * This is the common case of returning
		 * an integer as RPC result.
		 */
		l4_ipc_reply_and_wait(
		            _caller.dst(),
		            _write_offset <= 2*sizeof(umword_t) ? L4_IPC_SHORT_MSG : _snd_msg.msg_start(),
		            _caller.local_name(),
		            *reinterpret_cast<l4_umword_t *>(&_snd_msg.buf[sizeof(umword_t)]),
		            &_rcv_cs.caller, _rcv_msg.msg_start(),
		            reinterpret_cast<l4_umword_t *>(&_rcv_msg.buf[0]),
		            reinterpret_cast<l4_umword_t *>(&_rcv_msg.buf[sizeof(umword_t)]),
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
			wait();
		}

	} else wait();

	/* define destination of next reply */
	_caller = Native_capability(_rcv_cs.caller, 0);
	_badge  = reinterpret_cast<unsigned long *>(_rcv_msg.buf)[0];

	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg),
	Native_capability(Fiasco::l4_myself(), 0),
	_reply_needed(false), _rcv_cs(cs)
{
	_read_offset = _write_offset = sizeof(umword_t);
}


Ipc_server::~Ipc_server() { }
