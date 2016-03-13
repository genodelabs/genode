/*
 * \brief  Implementation of the IPC API for Fiasco.OC
 * \author Stefan Kalkowski
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
	DEBUG_MSG     = 0,
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
 */
static void copy_utcb_to_msgbuf(l4_msgtag_t tag, Msgbuf_base &rcv_msg)
{
	unsigned num_msg_words = l4_msgtag_words(tag);
	unsigned num_cap_sel   = l4_msgtag_items(tag);
	if (num_msg_words == 0 && num_cap_sel == 0)
		return;

	/* look up and validate destination message buffer to receive the payload */
	l4_mword_t *msg_buf = (l4_mword_t *)rcv_msg.buf;
	if (num_msg_words*sizeof(l4_mword_t) > rcv_msg.size()) {
		if (DEBUG_MSG)
			outstring("receive message buffer too small");
		num_msg_words = rcv_msg.size()/sizeof(l4_mword_t);
	}

	/* read message payload into destination message buffer */
	l4_mword_t *src = (l4_mword_t *)l4_utcb_mr();
	l4_mword_t *dst = (l4_mword_t *)&msg_buf[0];
	for (unsigned i = 0; i < num_msg_words; i++)
		*dst++ = *src++;

	rcv_msg.rcv_reset();
}


/**
 * Copy message registers from message buffer to UTCB and create message tag.
 */
static l4_msgtag_t copy_msgbuf_to_utcb(Msgbuf_base &snd_msg, unsigned offset,
                                       Native_capability dst)
{
	l4_mword_t *msg_buf       = (l4_mword_t *)snd_msg.buf;
	unsigned    num_msg_words = offset/sizeof(l4_mword_t);
	unsigned    num_cap_sel   = snd_msg.snd_cap_sel_cnt();

	if (num_msg_words + 2 * num_cap_sel > L4_UTCB_GENERIC_DATA_SIZE) {
		if (DEBUG_MSG)
			outstring("receive message buffer too small");
		throw Ipc_error();
	}

	/* first copy target label to message buffer */
	msg_buf[0] = dst.local_name();

	/* store message into UTCB message registers */
	for (unsigned i = 0; i < num_msg_words; i++)
		l4_utcb_mr()->mr[i] = msg_buf[i];

	/* setup flexpages of capabilities to send */
	for (unsigned i = 0; i < num_cap_sel; i++) {
		unsigned idx = num_msg_words + 2*i;
		l4_utcb_mr()->mr[idx]     = L4_ITEM_MAP/* | L4_ITEM_CONT*/;
		l4_utcb_mr()->mr[idx + 1] = l4_obj_fpage(snd_msg.snd_cap_sel(i),
		                                         0, L4_FPAGE_RWX).raw;
	}

	/* we have consumed capability selectors, reset message buffer */
	snd_msg.snd_reset();

	return l4_msgtag(0, num_msg_words, num_cap_sel, 0);
}


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	/* copy call message to the UTCBs message registers */
	l4_msgtag_t tag = copy_msgbuf_to_utcb(_snd_msg, _write_offset, _dst);

	addr_t rcv_cap_sel = _rcv_msg.rcv_cap_sel_base();
	for (int i = 0; i < Msgbuf_base::MAX_CAP_ARGS; i++) {
		l4_utcb_br()->br[i] = rcv_cap_sel | L4_RCV_ITEM_SINGLE_CAP;
		rcv_cap_sel += L4_CAP_SIZE;
	}

	tag = l4_ipc_call(_dst.dst(), l4_utcb(), tag, L4_IPC_NEVER);
	if (l4_ipc_error(tag, l4_utcb()) == L4_IPC_RECANCELED)
		throw Genode::Blocking_canceled();
	if (ipc_error(tag, DEBUG_MSG))
		throw Genode::Ipc_error();

	/* copy request message from the UTCBs message registers */
	copy_utcb_to_msgbuf(tag, _rcv_msg);

	_write_offset = _read_offset = sizeof(umword_t);
}


Ipc_client::Ipc_client(Native_capability const &dst,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg, unsigned short)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg), _result(0), _dst(dst)
{
	_read_offset = _write_offset = sizeof(l4_mword_t);
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
		addr_t rcv_cap_sel = _rcv_msg.rcv_cap_sel_base();
		for (int i = 0; i < Msgbuf_base::MAX_CAP_ARGS; i++) {
			l4_utcb_br()->br[i] = rcv_cap_sel | L4_RCV_ITEM_SINGLE_CAP;
			rcv_cap_sel += L4_CAP_SIZE;
		}
		l4_utcb_br()->bdr = 0;

		l4_msgtag_t tag;
		do {
			l4_umword_t label = 0;
			tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
			_rcv_msg.label(label);
		} while (ipc_error(tag, DEBUG_MSG));

		/* copy message from the UTCBs message registers to the receive buffer */
		copy_utcb_to_msgbuf(tag, _rcv_msg);

		/* reset unmarshaller */
		_read_offset = sizeof(l4_mword_t);

	} catch (Blocking_canceled) { }

	_badge = *reinterpret_cast<unsigned long *>(_rcv_msg.data());

	_prepare_next_reply_wait();
}


void Ipc_server::reply()
{
	l4_msgtag_t tag = copy_msgbuf_to_utcb(_snd_msg, _write_offset, Native_capability());
	tag = l4_ipc_send(L4_SYSF_REPLY, l4_utcb(), tag, L4_IPC_SEND_TIMEOUT_0);
	ipc_error(tag, DEBUG_MSG);
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

		l4_msgtag_t tag = copy_msgbuf_to_utcb(_snd_msg, _write_offset, Native_capability());
		tag = l4_ipc_reply_and_wait(l4_utcb(), tag, &label, L4_IPC_SEND_TIMEOUT_0);
		_rcv_msg.label(label);
		if (ipc_error(tag, false)) {
			/*
			 * The error conditions could be a message cut (which
			 * we want to ignore on the server side) or a reply failure
			 * (for example, if the caller went dead during the call.
			 * In both cases, we do not reflect the error condition to
			 * the user but want to wait for the next proper incoming
			 * message. So let's just wait now.
			 */
			wait();
		} else {

			/* copy request message from the UTCBs message registers */
			copy_utcb_to_msgbuf(tag, _rcv_msg);
		}
	} else
		wait();

	_badge = *reinterpret_cast<unsigned long *>(_rcv_msg.data());

	_prepare_next_reply_wait();
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg),
	Native_capability((Cap_index*)Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE]),
	_reply_needed(false), _rcv_cs(cs)
{
	_read_offset = _write_offset = sizeof(l4_mword_t);
}


Ipc_server::~Ipc_server() { }
