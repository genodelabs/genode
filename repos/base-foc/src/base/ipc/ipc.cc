/*
 * \brief  Implementation of the IPC API for Fiasco.OC
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2009-12-03
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * l4_msgtag_t (size == 1 mword) format:
 *
 *   --------------------------------------------------------------
 *  |  label  |  4 Bit flags  |  6 Bit items  |  6 Bit word count  |
 *   --------------------------------------------------------------
 */


/* Genode includes */
#include <base/blocking.h>
#include <base/ipc.h>
#include <base/ipc_msgbuf.h>
#include <base/thread.h>
#include <util/assert.h>

/* base-internal includes */
#include <base/internal/lock_helper.h> /* for 'thread_get_my_native_id()' */
#include <base/internal/ipc_server.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/consts.h>
#include <l4/sys/ipc.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
#include <l4/sys/kdebug.h>
}

using namespace Genode;
using namespace Fiasco;


/*****************************
 ** IPC marshalling support **
 *****************************/

void Ipc_marshaller::insert(Native_capability const &cap)
{
	if (cap.valid()) {
		if (!l4_msgtag_label(l4_task_cap_valid(L4_BASE_TASK_CAP, cap.dst()))) {
			insert(0UL);
			return;
		}
	}

	/* transfer capability id */
	insert(cap.local_name());

	/* only transfer kernel-capability if it's a valid one */
	if (cap.valid())
		_snd_msg.snd_append_cap_sel(cap.dst());

	ASSERT(!cap.valid() ||
	       l4_msgtag_label(l4_task_cap_valid(L4_BASE_TASK_CAP, cap.dst())),
	       "Send invalid cap");
}


void Ipc_unmarshaller::extract(Native_capability &cap)
{
	long value = 0;

	/* extract capability id from message buffer */
	extract(value);

	/* if id is zero an invalid capability was transfered */
	if (!value) {
		cap = Native_capability();
		return;
	}

	/* try to insert received capability in the map and return it */
	cap = Native_capability(cap_map()->insert_map(value, _rcv_msg.rcv_cap_sel()));
}


/***************
 ** Utilities **
 ***************/

enum Debug {
	DEBUG_MSG     = 1,
	HALT_ON_ERROR = 0
};


static inline bool ipc_error(l4_msgtag_t tag, bool print)
{
	int ipc_error = l4_ipc_error(tag, l4_utcb());
	if (ipc_error) {
		if (print) {
			outstring("Ipc error: ");
			outhex32(ipc_error);
			outstring(" occurred!\n");
		}
		if (HALT_ON_ERROR)
			enter_kdebug("Ipc error");
		return true;
	}
	return false;
}


/**
 * Copy message registers from UTCB to destination message buffer
 *
 * \return  protocol word (local name or exception code)
 */
static unsigned long extract_msg_from_utcb(l4_msgtag_t tag, Msgbuf_base &rcv_msg)
{
	unsigned const num_msg_words = l4_msgtag_words(tag);
	unsigned const num_cap_sel   = l4_msgtag_items(tag);

	/* each message has at least the protocol word */
	if (num_msg_words < 2 && num_cap_sel == 0)
		return 0;

	/* the first message word is reserved for the protocol word */
	unsigned num_data_msg_words = num_msg_words - 1;

	if ((num_data_msg_words)*sizeof(l4_mword_t) > rcv_msg.capacity()) {
		if (DEBUG_MSG)
			outstring("receive message buffer too small");
		num_data_msg_words = rcv_msg.capacity()/sizeof(l4_mword_t);
	}

	/* read protocol word from first UTCB message register */
	unsigned long const protocol_word = l4_utcb_mr()->mr[0];

	/* read message payload beginning from the second UTCB message register */
	l4_mword_t *src = (l4_mword_t *)l4_utcb_mr() + 1;
	l4_mword_t *dst = (l4_mword_t *)rcv_msg.data();
	for (unsigned i = 0; i < num_data_msg_words; i++)
		*dst++ = *src++;

	rcv_msg.rcv_reset();

	return protocol_word;
}


/**
 * Copy message registers from message buffer to UTCB and create message tag.
 *
 * \param protocol_word  badge of invoked object (when a client calls a server)
 *                       or the exception code (when a server replies to a
 *                       client)
 */
static l4_msgtag_t copy_msgbuf_to_utcb(Msgbuf_base &snd_msg,
                                       unsigned long protocol_word)
{
	unsigned const num_data_words = snd_msg.data_size() / sizeof(l4_mword_t);
	unsigned const num_msg_words  = num_data_words + 1;
	unsigned const num_cap_sel    = snd_msg.snd_cap_sel_cnt();

	/* account for message words, local name, and capability arguments */
	if (num_msg_words + 2*num_cap_sel > L4_UTCB_GENERIC_DATA_SIZE) {
		if (DEBUG_MSG)
			outstring("receive message buffer too small");
		throw Ipc_error();
	}

	/* copy badge / exception code to UTCB message register */
	l4_utcb_mr()->mr[0] = protocol_word;

	/* store message data into UTCB message registers */
	for (unsigned i = 0; i < num_data_words; i++)
		l4_utcb_mr()->mr[i + 1] = snd_msg.word(i);

	/* setup flexpages of capabilities to send */
	for (unsigned i = 0; i < num_cap_sel; i++) {
		unsigned const idx = num_msg_words + 2*i;
		l4_utcb_mr()->mr[idx]     = L4_ITEM_MAP/* | L4_ITEM_CONT*/;
		l4_utcb_mr()->mr[idx + 1] = l4_obj_fpage(snd_msg.snd_cap_sel(i),
		                                         0, L4_FPAGE_RWX).raw;
	}

	return l4_msgtag(0, num_msg_words, num_cap_sel, 0);
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t rcv_caps)
{
	/* copy call message to the UTCBs message registers */
	l4_msgtag_t const call_tag = copy_msgbuf_to_utcb(snd_msg, dst.local_name());

	addr_t rcv_cap_sel = rcv_msg.rcv_cap_sel_base();
	for (int i = 0; i < Msgbuf_base::MAX_CAP_ARGS; i++) {
		l4_utcb_br()->br[i] = rcv_cap_sel | L4_RCV_ITEM_SINGLE_CAP;
		rcv_cap_sel += L4_CAP_SIZE;
	}

	l4_msgtag_t const reply_tag =
		l4_ipc_call(dst.dst(), l4_utcb(), call_tag, L4_IPC_NEVER);

	if (l4_ipc_error(reply_tag, l4_utcb()) == L4_IPC_RECANCELED)
		throw Genode::Blocking_canceled();

	if (ipc_error(reply_tag, DEBUG_MSG))
		throw Genode::Ipc_error();

	return Rpc_exception_code(extract_msg_from_utcb(reply_tag, rcv_msg));
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_prepare_next_reply_wait()
{
	_reply_needed = true;
	_read_offset = _write_offset = 0;

	_snd_msg.snd_reset();
}


static unsigned long wait(Msgbuf_base &rcv_msg)
{
	addr_t rcv_cap_sel = rcv_msg.rcv_cap_sel_base();
	for (int i = 0; i < Msgbuf_base::MAX_CAP_ARGS; i++) {
		l4_utcb_br()->br[i] = rcv_cap_sel | L4_RCV_ITEM_SINGLE_CAP;
		rcv_cap_sel += L4_CAP_SIZE;
	}
	l4_utcb_br()->bdr = 0;

	l4_msgtag_t tag;
	do {
		l4_umword_t label = 0;
		tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
		rcv_msg.label(label);
	} while (ipc_error(tag, DEBUG_MSG));

	/* copy message from the UTCBs message registers to the receive buffer */
	return extract_msg_from_utcb(tag, rcv_msg);
}


void Ipc_server::reply()
{
	l4_msgtag_t tag = copy_msgbuf_to_utcb(_snd_msg, _exception_code.value);

	tag = l4_ipc_send(L4_SYSF_REPLY, l4_utcb(), tag, L4_IPC_SEND_TIMEOUT_0);

	ipc_error(tag, DEBUG_MSG);
	_snd_msg.snd_reset();
}


void Ipc_server::reply_wait()
{
	if (_reply_needed) {
		addr_t rcv_cap_sel = _rcv_msg.rcv_cap_sel_base();
		for (int i = 0; i < Msgbuf_base::MAX_CAP_ARGS; i++) {
			l4_utcb_br()->br[i] = rcv_cap_sel | L4_RCV_ITEM_SINGLE_CAP;
			rcv_cap_sel += L4_CAP_SIZE;
		}

		l4_utcb_br()->bdr &= ~L4_BDR_OFFSET_MASK;

		l4_umword_t label = 0;

		l4_msgtag_t const reply_tag =
			copy_msgbuf_to_utcb(_snd_msg, _exception_code.value);

		l4_msgtag_t const request_tag =
			l4_ipc_reply_and_wait(l4_utcb(), reply_tag, &label,
			                      L4_IPC_SEND_TIMEOUT_0);

		_rcv_msg.label(label);

		if (ipc_error(request_tag, false)) {
			/*
			 * The error conditions could be a message cut (which
			 * we want to ignore on the server side) or a reply failure
			 * (for example, if the caller went dead during the call.
			 * In both cases, we do not reflect the error condition to
			 * the user but want to wait for the next proper incoming
			 * message. So let's just wait now.
			 */
			_badge = wait(_rcv_msg);
		} else {

			/* copy request message from the UTCBs message registers */
			_badge = extract_msg_from_utcb(request_tag, _rcv_msg);
		}
	} else
		_badge = wait(_rcv_msg);

	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg),
	Native_capability((Cap_index*)Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE]),
	_rcv_cs(cs)
{ }


Ipc_server::~Ipc_server() { }
