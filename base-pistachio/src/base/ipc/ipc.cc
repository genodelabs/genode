/*
 * \brief  IPC implementation for Pistachio
 * \author Julian Stecklina
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/ipc.h>
#include <base/blocking.h>
#include <base/sleep.h>

namespace Pistachio {
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/kdebug.h>
}

using namespace Genode;
using namespace Pistachio;

#define VERBOSE_IPC 0
#if VERBOSE_IPC

/* Just a printf wrapper for now. */
#define IPCDEBUG(msg, ...) { \
	if (L4_Myself().raw == 0xf4001) { \
		(void)printf("IPC (thread = 0x%x) " msg, \
		L4_ThreadNo(Pistachio::L4_Myself()) \
		, ##__VA_ARGS__); \
	} else {}
}
#else
#define IPCDEBUG(...)
#endif

/*****************
 ** Ipc_ostream **
 *****************/

void Ipc_ostream::_send()
{
	IPCDEBUG("_send to 0x%08lx.\n", Ipc_ostream::_dst.dst().raw);

	L4_Msg_t msg;
	L4_StringItem_t sitem = L4_StringItem(_write_offset, _snd_msg->buf);
	L4_Word_t local_name = Ipc_ostream::_dst.local_name();

	L4_Clear(&msg);

	L4_Append(&msg, local_name);
	L4_Append(&msg, sitem);
	L4_Load(&msg);

	L4_MsgTag_t result = L4_Send(Ipc_ostream::_dst.dst());

	/*
	 * Error indicator
	 * TODO Check what happened and print a nicer error message.
	 */
	if (L4_IpcFailed(result)) {
		PERR("ipc error in _send.");
		throw Genode::Ipc_error();
	}

	IPCDEBUG("_send successful\n");
	_write_offset = sizeof(umword_t);
}


Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg) :
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = sizeof(umword_t);
	IPCDEBUG("Ipc_ostream constructed.\n");
}


/*****************
 ** Ipc_istream **
 *****************/

/**
 * Assert that we got 1 untyped word and 2 typed words
 */
static inline void check_ipc_result(L4_MsgTag_t result, L4_Word_t error_code)
{
	/*
	 * Test for IPC cancellation via Core's cancel-blocking mechanism
	 */
	enum { ERROR_MASK = 0xe, ERROR_CANCELED = 3 << 1 };
	if (L4_IpcFailed(result) &&
	    ((L4_ErrorCode() & ERROR_MASK) == ERROR_CANCELED))
		throw Genode::Blocking_canceled();

	/*
	 * Provide diagnostic information on unexpected conditions
	 */
	if (L4_IpcFailed(result)) {
		PERR("Error in thread %08lx. IPC failed.", L4_Myself().raw);
		throw Genode::Ipc_error();
	}

	if (L4_UntypedWords(result) != 1) {
		PERR("Error in thread %08lx. Expected one untyped word (local_name), but got %lu.\n",
		     L4_Myself().raw, L4_UntypedWords(result));

		PERR("This should not happen. Inspect!");
		throw Genode::Ipc_error();
	}
	if (L4_TypedWords(result) != 2) {
		PERR("Error. Expected two typed words (a string item). but got %lu.\n",
		     L4_TypedWords(result));
		PERR("This should not happen. Inspect!");
		throw Genode::Ipc_error();
	}
}


void Ipc_istream::_wait()
{
	L4_MsgTag_t result;
	L4_MsgBuffer_t msgbuf;

	IPCDEBUG("_wait.\n");
retry:

	IPCDEBUG("_wait loop start (more than once means IpcError)\n");

	L4_Clear (&msgbuf);
	L4_Append (&msgbuf, L4_StringItem (_rcv_msg->size(), _rcv_msg->buf));
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_Accept(L4_StringItemsAcceptor, &msgbuf);

	// Wait for message.
	result = L4_Wait(&_rcv_cs);

	if (L4_IpcFailed(result))
		goto retry;

	IPCDEBUG("Got something from 0x%x.\n", _rcv_cs);
	L4_Msg_t msg;

	L4_Store(result, &msg);

	check_ipc_result(result, L4_ErrorCode());

	/* get the local name */
	L4_Word_t local_name = L4_Get(&msg,0);

	/*
	 * Store local_name where badge() looks for it.
	 * XXX Check this...
	 */
	*((long *)_rcv_msg->buf) = local_name;
	_read_offset = sizeof(umword_t);

	IPCDEBUG("_wait successful\n");
}


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg) :
	Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()),
	Native_capability(Pistachio::L4_Myself(), 0),
	_rcv_msg(rcv_msg)
{
	IPCDEBUG("Ipc_istream constructed.\n");
	_rcv_cs = L4_nilthread;
	_read_offset = sizeof(umword_t);
}


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	IPCDEBUG("Starting to _call (with %u bytes of data).\n", _write_offset);
	L4_Msg_t msg;
	L4_StringItem_t sitem = L4_StringItem(_write_offset, _snd_msg->buf);
	L4_Word_t local_name = Ipc_ostream::_dst.local_name();

	IPCDEBUG("Destination local_name = 0x%x\n", local_name);

	L4_MsgBuffer_t msgbuf;

	/* prepare message buffer */
	L4_Clear (&msgbuf);
	L4_Append (&msgbuf, L4_StringItem (_rcv_msg->size(), _rcv_msg->buf));
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_Accept(L4_StringItemsAcceptor, &msgbuf);

	/* prepare sending parameters */
	L4_Clear(&msg);
	L4_Append(&msg, local_name);
	L4_Append(&msg, sitem);
	L4_Load(&msg);

	L4_MsgTag_t result = L4_Call(Ipc_ostream::_dst.dst());

	_write_offset = _read_offset = sizeof(umword_t);

	check_ipc_result(result, L4_ErrorCode());

	IPCDEBUG("Call done.\n");
}


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                                                     Msgbuf_base *rcv_msg) :
	Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{
	IPCDEBUG("Ipc_client constructed.\n");
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
	L4_Msg_t msg;
	L4_StringItem_t sitem = L4_StringItem(_write_offset, _snd_msg->buf);
	L4_Word_t local_name = Ipc_ostream::_dst.local_name();

	L4_Clear(&msg);
	L4_Append(&msg, local_name);
	L4_Append(&msg, sitem);
	L4_Load(&msg);

	L4_MsgTag_t result = L4_Reply(Ipc_ostream::_dst.dst());
	if (L4_IpcFailed(result))
		PERR("ipc error in _reply, ignored");

	_prepare_next_reply_wait();
}


void Ipc_server::_reply_wait()
{
	IPCDEBUG("Starting to _reply_wait. (with %u bytes of data)\n",
	         _reply_needed ? _write_offset : 0);

	if (_reply_needed) {

		/* prepare massage */
		L4_Msg_t msg;
		L4_StringItem_t sitem = L4_StringItem(_write_offset, _snd_msg->buf);
		L4_Word_t local_name = Ipc_ostream::_dst.local_name();

		L4_Clear(&msg);
		L4_Append(&msg, local_name);
		L4_Append(&msg, sitem);
		L4_Load(&msg);

		/* Prepare message buffer */
		L4_MsgBuffer_t msgbuf;
		L4_Clear(&msgbuf);
		L4_Append(&msgbuf, L4_StringItem (_rcv_msg->size(), _rcv_msg->buf));
		L4_Accept(L4_UntypedWordsAcceptor);
		L4_Accept(L4_StringItemsAcceptor, &msgbuf);

		L4_MsgTag_t result = L4_Ipc(Ipc_ostream::_dst.dst(), L4_anythread,
		                            L4_Timeouts(L4_ZeroTime, L4_Never), &_rcv_cs);
		IPCDEBUG("Got something from 0x%x.\n", L4_ThreadNo(L4_GlobalId(_rcv_cs)));

		/* error handling - check whether send or receive failed */
		if (L4_IpcFailed(result)) {
			L4_Word_t errcode = L4_ErrorCode();
			L4_Word_t phase   = errcode & 1;
			L4_Word_t error   = (errcode & 0xF) >> 1;

			PERR("IPC %s error %02lx, offset %08lx -> _wait() instead.",
			     phase ? "receive" : "send", error, errcode >> 4);
			_wait();
			return;
		}

		L4_Clear(&msg);
		L4_Store(result, &msg);

		try {
			check_ipc_result(result, L4_ErrorCode());
		} catch (...) {
			/*
			 * If something went wrong, just call _wait instead of relaying
			 * the error to the user.
			 */
			IPCDEBUG("Bad IPC content -> _wait() instead.\n");
			_wait();
			return;
		}

		/* get the local name */
		local_name = L4_Get(&msg, 0);

		/*
		 * Store local_name where badge() looks for it.
		 * XXX Check this...
		 */
		*((long *)_rcv_msg->buf) = local_name;
		IPCDEBUG("local_name = 0x%lx\n", badge());

		/* define destination of next reply */
		Ipc_ostream::_dst = Native_capability(_rcv_cs, badge());

		_prepare_next_reply_wait();

	} else
		_wait();
}


Ipc_server::Ipc_server(Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg) :
	Ipc_istream(rcv_msg),
	Ipc_ostream(Native_capability(), snd_msg),
	_reply_needed(false)
{ }
