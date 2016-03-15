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
 * exception code (when the server replies to the client). All subsequent
 * message registers hold the message payload.
 */

/**
 * Copy message registers from UTCB to destination message buffer
 *
 * \return  local name / exception code
 */
static L4_Word_t extract_msg_from_utcb(L4_MsgTag_t rcv_tag, Msgbuf_base &rcv_msg)
{
	unsigned num_msg_words = (int)L4_UntypedWords(rcv_tag);

	if (num_msg_words*sizeof(L4_Word_t) > rcv_msg.capacity()) {
		PERR("receive message buffer too small msg size=%zd, buf size=%zd",
		     num_msg_words*sizeof(L4_Word_t), rcv_msg.capacity());
		num_msg_words = rcv_msg.capacity()/sizeof(L4_Word_t);
	}

	L4_Word_t local_name = 0;
	L4_StoreMR(1, &local_name);

	/* read message payload into destination message buffer */
	L4_StoreMRs(2, num_msg_words - 2, (L4_Word_t *)rcv_msg.data());

	return local_name;
}


/**
 * Copy message payload to UTCB message registers
 */
static void copy_msg_to_utcb(Msgbuf_base const &snd_msg, unsigned num_msg_words,
                             L4_Word_t local_name)
{
	num_msg_words += 2;

	if (num_msg_words >= L4_GetMessageRegisters()) {
		kdb_emergency_print("Message does not fit into UTCB message registers\n");
		num_msg_words = L4_GetMessageRegisters() - 1;
	}

	L4_MsgTag_t snd_tag;
	snd_tag.raw = 0;
	snd_tag.X.u = num_msg_words;
	L4_LoadMR (0, snd_tag.raw);
	L4_LoadMR (1, local_name);
	L4_LoadMRs(2, num_msg_words - 2, (L4_Word_t *)snd_msg.data());
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t)
{
	/* copy call message to the UTCBs message registers */
	copy_msg_to_utcb(snd_msg, snd_msg.data_size()/sizeof(L4_Word_t),
	                 dst.local_name());

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
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	_reply_needed = true;
	_read_offset = _write_offset = 0;
}


void Ipc_server::reply()
{
	/* copy reply to the UTCBs message registers */
	copy_msg_to_utcb(_snd_msg, _write_offset/sizeof(L4_Word_t),
	                 _exception_code.value);

	/* perform non-blocking IPC send operation */
	L4_MsgTag_t rcv_tag = L4_Reply(_caller.dst());

	if (L4_IpcFailed(rcv_tag))
		PERR("ipc error in _reply - gets ignored");

	_prepare_next_reply_wait();
}


void Ipc_server::reply_wait()
{
	L4_MsgTag_t rcv_tag;

	if (_reply_needed) {

		/* copy reply to the UTCBs message registers */
		copy_msg_to_utcb(_snd_msg, _write_offset/sizeof(L4_Word_t),
		                 _exception_code.value);

		rcv_tag = L4_ReplyWait(_caller.dst(), &_rcv_cs.caller);
	} else {
		rcv_tag = L4_Wait(&_rcv_cs.caller);
	}

	/* copy request message from the UTCBs message registers */
	_badge = extract_msg_from_utcb(rcv_tag, _rcv_msg);

	/* define destination of next reply */
	_caller = Native_capability(_rcv_cs.caller, badge());

	_prepare_next_reply_wait();
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


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg),
	Ipc_unmarshaller(rcv_msg),
	Native_capability(thread_get_my_global_id(), 0),
	_rcv_cs(cs)
{ }


Ipc_server::~Ipc_server() { }
