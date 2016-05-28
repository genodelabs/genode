/*
 * \brief  IPC implementation for OKL4
 * \author Norman Feske
 * \date   2009-03-25
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/ipc.h>
#include <base/native_types.h>
#include <base/blocking.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/config.h>
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/kdebug.h>
} }

using namespace Genode;
using namespace Okl4;


/***************
 ** Utilities **
 ***************/

/**
 * Print string, bypassing Genode's LOG mechanism
 *
 * This function is used in conditions where Genode's base mechanisms may fail.
 */
static void kdb_emergency_print(const char *s)
{
	for (; s && *s; s++)
		Okl4::L4_KDB_PrintChar(*s);
}

/*
 * Message layout within the UTCB
 *
 * The message tag contains the information about the number of message words
 * to send. The tag is always supplied in message register 0. Message register
 * 1 is used for the local name (when the client calls the server) or the
 * exception code (when the server replies to the client). Message register
 * 2 holds the number of transferred capability arguments. It is followed
 * by a tuple of (thread ID, local name) for each capability. All subsequent
 * message registers hold the message payload.
 */

/**
 * Copy message registers from UTCB to destination message buffer
 *
 * \return  local name / exception code
 */
static L4_Word_t extract_msg_from_utcb(L4_MsgTag_t rcv_tag, Msgbuf_base &rcv_msg)
{
	rcv_msg.reset();

	unsigned const num_msg_words = (int)L4_UntypedWords(rcv_tag);

	if (num_msg_words < 3)
		return Rpc_exception_code::INVALID_OBJECT;

	L4_Word_t protocol_word = 0;
	L4_StoreMR(1, &protocol_word);

	L4_Word_t num_caps = 0;
	L4_StoreMR(2, &num_caps);

	/* each capability is represened as two message words (tid, local name) */
	for (unsigned i = 0; i < num_caps; i++) {
		L4_Word_t local_name = 0;
		L4_ThreadId_t tid;
		L4_StoreMR(3 + 2*i,     &tid.raw);
		L4_StoreMR(3 + 2*i + 1, &local_name);
		rcv_msg.insert(Native_capability(tid, local_name));
	}

	unsigned const data_start_idx = 3 + 2*num_caps;

	if (num_msg_words < data_start_idx)
		return Rpc_exception_code::INVALID_OBJECT;

	unsigned const num_data_words = num_msg_words - data_start_idx;

	if (num_data_words*sizeof(L4_Word_t) > rcv_msg.capacity()) {
		PERR("receive message buffer too small msg size=%zd, buf size=%zd",
		     num_data_words*sizeof(L4_Word_t), rcv_msg.capacity());
		return Rpc_exception_code::INVALID_OBJECT;
	}

	/* read message payload into destination message buffer */
	L4_StoreMRs(data_start_idx, num_data_words, (L4_Word_t *)rcv_msg.data());

	rcv_msg.data_size(sizeof(num_data_words));

	return protocol_word;
}


/**
 * Copy message payload to UTCB message registers
 */
static void copy_msg_to_utcb(Msgbuf_base const &snd_msg, L4_Word_t local_name)
{
	unsigned const num_header_words = 3 + 2*snd_msg.used_caps();
	unsigned const num_data_words   = snd_msg.data_size()/sizeof(L4_Word_t);
	unsigned const num_msg_words    = num_data_words + num_header_words;

	if (num_msg_words >= L4_GetMessageRegisters()) {
		kdb_emergency_print("Message does not fit into UTCB message registers\n");
		L4_LoadMR(0, 0);
		return;
	}

	L4_MsgTag_t snd_tag;
	snd_tag.raw = 0;
	snd_tag.X.u = num_msg_words;

	L4_LoadMR(0, snd_tag.raw);
	L4_LoadMR(1, local_name);
	L4_LoadMR(2, snd_msg.used_caps());

	for (unsigned i = 0; i < snd_msg.used_caps(); i++) {
		L4_LoadMR(3 + i*2,     snd_msg.cap(i).dst().raw);
		L4_LoadMR(3 + i*2 + 1, snd_msg.cap(i).local_name());
	}

	L4_LoadMRs(num_header_words, num_data_words, (L4_Word_t *)snd_msg.data());
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t)
{
	/* copy call message to the UTCBs message registers */
	copy_msg_to_utcb(snd_msg, dst.local_name());

	L4_Accept(L4_UntypedWordsAcceptor);

	L4_MsgTag_t rcv_tag = L4_Call(dst.dst());

	enum { ERROR_MASK = 0xe, ERROR_CANCELED = 3 << 1 };
	if (L4_IpcFailed(rcv_tag) &&
	    ((L4_ErrorCode() & ERROR_MASK) == ERROR_CANCELED))
		throw Genode::Blocking_canceled();

	if (L4_IpcFailed(rcv_tag)) {
		kdb_emergency_print("Ipc failed\n");

		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);
	}

	return Rpc_exception_code(extract_msg_from_utcb(rcv_tag, rcv_msg));
}


/****************
 ** IPC server **
 ****************/

void Genode::ipc_reply(Native_capability caller, Rpc_exception_code exc,
                       Msgbuf_base &snd_msg)
{
	/* copy reply to the UTCBs message registers */
	copy_msg_to_utcb(snd_msg, exc.value);

	/* perform non-blocking IPC send operation */
	L4_MsgTag_t rcv_tag = L4_Reply(caller.dst());

	if (L4_IpcFailed(rcv_tag))
		PERR("ipc error in _reply - gets ignored");
}


Genode::Rpc_request Genode::ipc_reply_wait(Reply_capability const &last_caller,
                                           Rpc_exception_code      exc,
                                           Msgbuf_base            &reply_msg,
                                           Msgbuf_base            &request_msg)
{
	L4_MsgTag_t rcv_tag;

	Okl4::L4_ThreadId_t caller = L4_nilthread;

	if (last_caller.valid()) {

		/* copy reply to the UTCBs message registers */
		copy_msg_to_utcb(reply_msg, exc.value);

		rcv_tag = L4_ReplyWait(last_caller.dst(), &caller);
	} else {
		rcv_tag = L4_Wait(&caller);
	}

	/* copy request message from the UTCBs message registers */
	unsigned long const badge = extract_msg_from_utcb(rcv_tag, request_msg);

	return Rpc_request(Native_capability(caller, 0), badge);
}


/**
 * Return the global thread ID of the calling thread
 *
 * On OKL4 we cannot use 'L4_Myself()' to determine our own thread's
 * identity. By convention, each thread stores its global ID in a
 * defined entry of its UTCB.
 */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}


Ipc_server::Ipc_server() : Native_capability(thread_get_my_global_id(), 0) { }


Ipc_server::~Ipc_server() { }
