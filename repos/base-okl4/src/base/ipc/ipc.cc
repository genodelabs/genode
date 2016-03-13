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
#include <base/internal/native_connection_state.h>

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


/**
 * Copy message registers from UTCB to destination message buffer
 */
static void copy_utcb_to_msgbuf(L4_MsgTag_t rcv_tag, Msgbuf_base &rcv_msg)
{
	int num_msg_words = (int)L4_UntypedWords(rcv_tag);
	if (num_msg_words <= 0) return;

	/* look up and validate destination message buffer to receive the payload */
	L4_Word_t *msg_buf = (L4_Word_t *)rcv_msg.buf;
	if (num_msg_words*sizeof(L4_Word_t) > rcv_msg.size()) {
		PERR("receive message buffer too small msg size=%zd, buf size=%zd",
		     num_msg_words*sizeof(L4_Word_t), rcv_msg.size());
		num_msg_words = rcv_msg.size()/sizeof(L4_Word_t);
	}

	/* read message payload into destination message buffer */
	L4_StoreMRs(1, num_msg_words, msg_buf);
}


/**
 * Copy message payload to UTCB message registers
 *
 * The message tag contains the information about the number of message words
 * to send. The tag is always supplied in message register 0. Message register
 * 1 is used for the local name. All subsequent message registers hold the
 * message payload.
 */
static void copy_msgbuf_to_utcb(Msgbuf_base &snd_msg, unsigned num_msg_words,
                                L4_Word_t local_name)
{
	/* look up address and size of message payload */
	L4_Word_t *msg_buf = (L4_Word_t *)snd_msg.buf;

	num_msg_words += 1;

	if (num_msg_words >= L4_GetMessageRegisters()) {
		kdb_emergency_print("Message does not fit into UTCB message registers\n");
		num_msg_words = L4_GetMessageRegisters() - 1;
	}

	L4_MsgTag_t snd_tag;
	snd_tag.raw = 0;
	snd_tag.X.u = num_msg_words;
	L4_LoadMR (0, snd_tag.raw);
	L4_LoadMR (1, local_name);
	L4_LoadMRs(2, num_msg_words - 1, msg_buf + 1);
}


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	/* copy call message to the UTCBs message registers */
	copy_msgbuf_to_utcb(_snd_msg, _write_offset/sizeof(L4_Word_t),
	                    _dst.local_name());

	L4_Accept(L4_UntypedWordsAcceptor);
	L4_MsgTag_t rcv_tag = L4_Call(_dst.dst());

	enum { ERROR_MASK = 0xe, ERROR_CANCELED = 3 << 1 };
	if (L4_IpcFailed(rcv_tag) &&
	    ((L4_ErrorCode() & ERROR_MASK) == ERROR_CANCELED))
		throw Genode::Blocking_canceled();

	if (L4_IpcFailed(rcv_tag)) {
		kdb_emergency_print("Ipc failed\n");
		/* set return value for ipc_generic part if call failed */
		ret(ERR_INVALID_OBJECT);
	}

	/* copy request message from the UTCBs message registers */
	copy_utcb_to_msgbuf(rcv_tag, _rcv_msg);

	_write_offset = _read_offset = sizeof(umword_t);
}


Ipc_client::Ipc_client(Native_capability const &dst,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg, unsigned short)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg), _result(0), _dst(dst)
{
	_write_offset = _read_offset = sizeof(umword_t);
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
		/*
		 * Wait for IPC message
		 *
		 * The message tag (holding the size of the message) is located at
		 * message register 0 and implicitly addressed by 'L4_UntypedWords()'.
		 */
		L4_MsgTag_t rcv_tag = L4_Wait(&_rcv_cs.caller);

		/* copy message from the UTCBs message registers to the receive buffer */
		copy_utcb_to_msgbuf(rcv_tag, _rcv_msg);

		/* reset unmarshaller */
		_read_offset = sizeof(umword_t);

	} catch (Blocking_canceled) { }

	/* define destination of next reply */
	_caller = Native_capability(_rcv_cs.caller, badge());
	_badge  = reinterpret_cast<unsigned long *>(_rcv_msg.data())[0];

	_prepare_next_reply_wait();
}


void Ipc_server::reply()
{
	/* copy reply to the UTCBs message registers */
	copy_msgbuf_to_utcb(_snd_msg, _write_offset/sizeof(L4_Word_t),
	                    _caller.local_name());

	/* perform non-blocking IPC send operation */
	L4_MsgTag_t rcv_tag = L4_Reply(_caller.dst());

	if (L4_IpcFailed(rcv_tag))
		PERR("ipc error in _reply - gets ignored");

	_prepare_next_reply_wait();
}


void Ipc_server::reply_wait()
{
	if (_reply_needed) {

		/* copy reply to the UTCBs message registers */
		copy_msgbuf_to_utcb(_snd_msg, _write_offset/sizeof(L4_Word_t),
		                    _caller.local_name());

		L4_MsgTag_t rcv_tag = L4_ReplyWait(_caller.dst(), &_rcv_cs.caller);

		/*
		 * TODO: Check for IPC error
		 */

		/* copy request message from the UTCBs message registers */
		copy_utcb_to_msgbuf(rcv_tag, _rcv_msg);

		/* define destination of next reply */
		_caller = Native_capability(_rcv_cs.caller, badge());
		_badge  = reinterpret_cast<unsigned long *>(_rcv_msg.data())[0];

		_prepare_next_reply_wait();

	} else
		wait();
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
	_reply_needed(false), _rcv_cs(cs)
{
	_read_offset = _write_offset = sizeof(umword_t);
}

Ipc_server::~Ipc_server() { }
