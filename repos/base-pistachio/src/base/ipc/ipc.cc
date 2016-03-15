/*
 * \brief  IPC implementation for Pistachio
 * \author Julian Stecklina
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/ipc.h>
#include <base/blocking.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>

/* Pistachio includes */
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


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t)
{
	L4_Msg_t msg;
	L4_StringItem_t sitem = L4_StringItem(snd_msg.data_size(), snd_msg.data());
	L4_Word_t const local_name = dst.local_name();

	L4_MsgBuffer_t msgbuf;

	/* prepare message buffer */
	L4_Clear (&msgbuf);
	L4_Append (&msgbuf, L4_StringItem (rcv_msg.capacity(), rcv_msg.data()));
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_Accept(L4_StringItemsAcceptor, &msgbuf);

	/* prepare sending parameters */
	L4_Clear(&msg);
	L4_Append(&msg, local_name);
	L4_Append(&msg, sitem);
	L4_Load(&msg);

	L4_MsgTag_t result = L4_Call(dst.dst());

	L4_Clear(&msg);
	L4_Store(result, &msg);

	check_ipc_result(result, L4_ErrorCode());

	return Rpc_exception_code(L4_Get(&msg, 0));
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	_reply_needed = true;
	_read_offset = _write_offset = 0;
}


void Ipc_server::reply()
{
	L4_Msg_t msg;
	L4_StringItem_t sitem = L4_StringItem(_write_offset, _snd_msg.data());
	L4_Word_t const local_name = _caller.local_name();

	L4_Clear(&msg);
	L4_Append(&msg, local_name);
	L4_Append(&msg, sitem);
	L4_Load(&msg);

	L4_MsgTag_t result = L4_Reply(_caller.dst());
	if (L4_IpcFailed(result))
		PERR("ipc error in _reply, ignored");

	_prepare_next_reply_wait();
}


void Ipc_server::reply_wait()
{
	bool need_to_wait = true;

	L4_MsgTag_t request_tag;

	/* prepare request message buffer */
	L4_MsgBuffer_t request_msgbuf;
	L4_Clear(&request_msgbuf);
	L4_Append(&request_msgbuf, L4_StringItem (_rcv_msg.capacity(), _rcv_msg.data()));
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_Accept(L4_StringItemsAcceptor, &request_msgbuf);

	if (_reply_needed) {

		/* prepare reply massage */
		L4_Msg_t reply_msg;
		L4_StringItem_t sitem = L4_StringItem(_write_offset, _snd_msg.data());

		L4_Clear(&reply_msg);
		L4_Append(&reply_msg, (L4_Word_t)_exception_code.value);
		L4_Append(&reply_msg, sitem);
		L4_Load(&reply_msg);

		/* send reply and wait for new request message */
		request_tag = L4_Ipc(_caller.dst(), L4_anythread,
		                     L4_Timeouts(L4_ZeroTime, L4_Never), &_rcv_cs.caller);

		if (!L4_IpcFailed(request_tag))
			need_to_wait = false;
	}

	while (need_to_wait) {

		/* wait for new request message */
		request_tag = L4_Wait(&_rcv_cs.caller);

		if (!L4_IpcFailed(request_tag))
			need_to_wait = false;
	}

	/* extract request parameters */
	L4_Msg_t msg;
	L4_Clear(&msg);
	L4_Store(request_tag, &msg);

	/* remember badge of invoked object */
	_badge = L4_Get(&msg, 0);

	/* define destination of next reply */
	_caller = Native_capability(_rcv_cs.caller, badge());

	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg),
	Native_capability(Pistachio::L4_Myself(), 0),
	_rcv_cs(cs)
{
	_read_offset = _write_offset = 0;
}


Ipc_server::~Ipc_server() { }
