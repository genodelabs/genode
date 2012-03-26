/*
 * \brief  IPC implementation for Fiasco
 * \author Norman Feske
 * \date   2006-06-13
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/ipc.h>
#include <base/blocking.h>

namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
}

using namespace Genode;


/*****************
 ** Ipc_ostream **
 *****************/

void Ipc_ostream::_send()
{
	using namespace Fiasco;

	_snd_msg->send_dope = L4_IPC_DOPE((_write_offset + sizeof(umword_t) - 1)>>2, 0);

	l4_msgdope_t result;
	l4_ipc_send(_dst.dst(), _snd_msg->addr(), _dst.local_name(),
	            *reinterpret_cast<l4_umword_t *>(&_snd_msg->buf[sizeof(umword_t)]),
	            L4_IPC_NEVER, &result);

	if (L4_IPC_IS_ERROR(result)) {
		PERR("Ipc error %lx", L4_IPC_ERROR(result));
		throw Genode::Ipc_error();
	}

	_write_offset = sizeof(umword_t);
}


Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg) :
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = sizeof(umword_t);
}


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait()
{
	using namespace Fiasco;

	l4_msgdope_t  result;

	/*
	 * Wait until we get a proper message and thereby
	 * ignore receive message cuts on the server-side.
	 * This error condition should be handled by the
	 * client. The server does not bother.
	 */
	do {
		_rcv_msg->size_dope = L4_IPC_DOPE(_rcv_msg->size()>>2, 0);

		l4_ipc_wait(&_rcv_cs, _rcv_msg->addr(),
		            reinterpret_cast<l4_umword_t *>(&_rcv_msg->buf[0]),
		            reinterpret_cast<l4_umword_t *>(&_rcv_msg->buf[sizeof(umword_t)]),
		            L4_IPC_NEVER, &result);

		if (L4_IPC_IS_ERROR(result))
			PERR("Ipc error %lx", L4_IPC_ERROR(result));

	} while (L4_IPC_IS_ERROR(result));

	/* reset buffer read offset */
	_read_offset = sizeof(umword_t);
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg):
	Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()),
	Native_capability(Fiasco::l4_myself(), 0),
	_rcv_msg(rcv_msg)
{
	using namespace Fiasco;

	_rcv_cs = L4_INVALID_ID;

	_read_offset = sizeof(umword_t);
}


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	using namespace Fiasco;

	l4_msgdope_t ipc_result;
	long         rec_badge;

	_snd_msg->send_dope = L4_IPC_DOPE((_write_offset + 2*sizeof(umword_t) - 1)>>2, 0);
	_rcv_msg->size_dope = L4_IPC_DOPE(_rcv_msg->size()>>2, 0);

	l4_ipc_call(Ipc_ostream::_dst.dst(),
	            _write_offset <= 2*sizeof(umword_t) ? L4_IPC_SHORT_MSG : _snd_msg->addr(),
	            Ipc_ostream::_dst.local_name(),
	            *reinterpret_cast<l4_umword_t *>(&_snd_msg->buf[sizeof(umword_t)]),
	            _rcv_msg->addr(),
	            reinterpret_cast<l4_umword_t *>(&rec_badge),
	            reinterpret_cast<l4_umword_t *>(&_rcv_msg->buf[sizeof(umword_t)]),
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


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                                                     Msgbuf_base *rcv_msg):
	Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{ }


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


void Ipc_server::_wait()
{
	/* wait for new server request */
	try { Ipc_istream::_wait(); } catch (Blocking_canceled) { }

	/* define destination of next reply */
	Ipc_ostream::_dst = Native_capability(_rcv_cs, badge());

	_prepare_next_reply_wait();
}


void Ipc_server::_reply()
{
	using namespace Fiasco;

	_snd_msg->send_dope = L4_IPC_DOPE((_write_offset + sizeof(umword_t) - 1)>>2, 0);

	l4_msgdope_t result;
	l4_ipc_send(Ipc_ostream::_dst.dst(), _snd_msg->addr(), 
	            Ipc_ostream::_dst.local_name(),
	            *reinterpret_cast<l4_umword_t *>(&_snd_msg->buf[sizeof(umword_t)]),
	            L4_IPC_SEND_TIMEOUT_0, &result);

	if (L4_IPC_IS_ERROR(result))
		PERR("Ipc error %lx, ignored", L4_IPC_ERROR(result));

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	using namespace Fiasco;

	if (_reply_needed) {

		l4_msgdope_t ipc_result;

		_snd_msg->send_dope = L4_IPC_DOPE((_write_offset + sizeof(umword_t) - 1)>>2, 0);
		_rcv_msg->size_dope = L4_IPC_DOPE(_rcv_msg->size()>>2, 0);

		/*
		 * Use short IPC for reply if possible.
		 * This is the common case of returning
		 * an integer as RPC result.
		 */
		l4_ipc_reply_and_wait(
		            Ipc_ostream::_dst.dst(),
		            _write_offset <= 2*sizeof(umword_t) ? L4_IPC_SHORT_MSG : _snd_msg->addr(),
		            Ipc_ostream::_dst.local_name(),
		            *reinterpret_cast<l4_umword_t *>(&_snd_msg->buf[sizeof(umword_t)]),
		            &_rcv_cs, _rcv_msg->addr(),
		            reinterpret_cast<l4_umword_t *>(&_rcv_msg->buf[0]),
		            reinterpret_cast<l4_umword_t *>(&_rcv_msg->buf[sizeof(umword_t)]),
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
			_wait();
		}

	} else _wait();

	/* define destination of next reply */
	Ipc_ostream::_dst = Native_capability(_rcv_cs, badge());

	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg):
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(), snd_msg), _reply_needed(false)
{ }
