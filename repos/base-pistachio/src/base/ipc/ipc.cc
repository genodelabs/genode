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

	if (L4_UntypedWords(result) < 2) {
		PERR("Error in thread %08lx. Expected at leat two untyped words, but got %lu.\n",
		     L4_Myself().raw, L4_UntypedWords(result));
		throw Genode::Ipc_error();
	}
}


/**
 * Configure L4 receive buffer according to Genode receive buffer
 */
static void extract_caps(L4_Msg_t &msg, Msgbuf_base &rcv_msg)
{
	using namespace Pistachio;

	L4_Word_t const num_caps = min((unsigned)Msgbuf_base::MAX_CAPS_PER_MSG,
	                               L4_Get(&msg, 1));
	for (unsigned i = 0; i < num_caps; i++) {

		L4_ThreadId_t tid;
		tid.raw = L4_Get(&msg, 2 + i*2);

		L4_Word_t const local_name = L4_Get(&msg, 2 + i*2 + 1);

		rcv_msg.insert(Native_capability(tid, local_name));
	}
}


/**
 * Configure L4 message descriptor according to Genode send buffer
 */
static void prepare_send(L4_Word_t protocol_value, L4_Msg_t &msg,
                         Msgbuf_base &snd_msg)
{
	L4_Clear(&msg);
	L4_Append(&msg, protocol_value);

	L4_Append(&msg, (L4_Word_t)snd_msg.used_caps());

	for (unsigned i = 0; i < snd_msg.used_caps(); i++) {
		L4_Append(&msg, (L4_Word_t)snd_msg.cap(i).dst().raw);
		L4_Append(&msg, (L4_Word_t)snd_msg.cap(i).local_name());
	}

	L4_Append(&msg, L4_StringItem(snd_msg.data_size(), snd_msg.data()));
	L4_Load(&msg);
}


static void prepare_receive(L4_MsgBuffer_t &l4_msgbuf, Msgbuf_base &rcv_msg)
{
	L4_Clear(&l4_msgbuf);
	L4_Append(&l4_msgbuf, L4_StringItem(rcv_msg.capacity(), rcv_msg.data()));
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_Accept(L4_StringItemsAcceptor, &l4_msgbuf);
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t)
{
	/* prepare receive message buffer */
	L4_MsgBuffer_t msgbuf;
	prepare_receive(msgbuf, rcv_msg);

	/* prepare sending parameters */
	L4_Msg_t msg;
	prepare_send(dst.local_name(), msg, snd_msg);

	L4_MsgTag_t result = L4_Call(dst.dst());

	L4_Clear(&msg);
	L4_Store(result, &msg);

	check_ipc_result(result, L4_ErrorCode());

	extract_caps(msg, rcv_msg);

	return Rpc_exception_code(L4_Get(&msg, 0));
}


/****************
 ** IPC server **
 ****************/

void Genode::ipc_reply(Native_capability caller, Rpc_exception_code exc,
                       Msgbuf_base &snd_msg)
{
	L4_Msg_t msg;
	prepare_send(exc.value, msg, snd_msg);

	L4_MsgTag_t result = L4_Reply(caller.dst());
	if (L4_IpcFailed(result))
		PERR("ipc error in _reply, ignored");

	snd_msg.reset();
}


Genode::Rpc_request Genode::ipc_reply_wait(Reply_capability const &last_caller,
                                           Rpc_exception_code      exc,
                                           Msgbuf_base            &reply_msg,
                                           Msgbuf_base            &request_msg)
{
	bool need_to_wait = true;

	L4_MsgTag_t request_tag;

	request_msg.reset();

	/* prepare receive buffer for request */
	L4_MsgBuffer_t request_msgbuf;
	prepare_receive(request_msgbuf, request_msg);

	L4_ThreadId_t caller = L4_nilthread;

	if (last_caller.valid() && exc.value != Rpc_exception_code::INVALID_OBJECT) {

		/* prepare reply massage */
		L4_Msg_t reply_l4_msg;
		prepare_send(exc.value, reply_l4_msg, reply_msg);

		/* send reply and wait for new request message */
		request_tag = L4_Ipc(last_caller.dst(), L4_anythread,
		                     L4_Timeouts(L4_ZeroTime, L4_Never), &caller);

		if (!L4_IpcFailed(request_tag))
			need_to_wait = false;
	}

	while (need_to_wait) {

		/* wait for new request message */
		request_tag = L4_Wait(&caller);

		if (!L4_IpcFailed(request_tag))
			need_to_wait = false;
	}

	/* extract request parameters */
	L4_Msg_t msg;
	L4_Clear(&msg);
	L4_Store(request_tag, &msg);
	extract_caps(msg, request_msg);

	unsigned long const badge = L4_Get(&msg, 0);
	return Rpc_request(Native_capability(caller, 0), badge);
}


Ipc_server::Ipc_server() : Native_capability(Pistachio::L4_Myself(), 0) { }


Ipc_server::~Ipc_server() { }
