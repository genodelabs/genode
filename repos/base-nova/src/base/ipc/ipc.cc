/*
 * \brief  Implementation of the IPC API for NOVA
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>
#include <base/printf.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;
using namespace Nova;


/*****************************
 ** IPC marshalling support **
 *****************************/

void Ipc_marshaller::insert(Native_capability const &cap)
{
	_snd_msg.snd_append_pt_sel(cap);
}


void Ipc_unmarshaller::extract(Native_capability &cap)
{
	_rcv_msg.rcv_pt_sel(cap);
}


/***************
 ** Utilities **
 ***************/

/**
 * Copy message registers from UTCB to destination message buffer
 *
 * \return  protocol word delivered via the first UTCB message register
 *
 * The caller of this function must ensure that utcb->msg_words is greater
 * than 0.
 */
static mword_t copy_utcb_to_msgbuf(Nova::Utcb *utcb, Msgbuf_base &rcv_msg)
{
	size_t num_msg_words = utcb->msg_words();

	/*
	 * Handle the reception of a malformed message. This should never happen
	 * because the utcb->msg_words is checked by the caller of this function.
	 */
	if (num_msg_words < 1)
		return 0;

	/* the UTCB contains the protocol word followed by the message data */
	mword_t const protocol_word  = utcb->msg[0];
	size_t        num_data_words = num_msg_words - 1;

	if (num_data_words*sizeof(mword_t) > rcv_msg.capacity()) {
		PERR("receive message buffer too small msg size=%zx, buf size=%zd",
		     num_data_words*sizeof(mword_t), rcv_msg.capacity());
		num_data_words = rcv_msg.capacity()/sizeof(mword_t);
	}

	/* read message payload into destination message buffer */
	mword_t *src = (mword_t *)(void *)(&utcb->msg[1]);
	mword_t *dst = (mword_t *)rcv_msg.data();
	for (unsigned i = 0; i < num_data_words; i++)
		*dst++ = *src++;

	return protocol_word;
}


/**
 * Copy message payload to UTCB message registers
 */
static bool copy_msgbuf_to_utcb(Nova::Utcb *utcb, Msgbuf_base const &snd_msg,
                                mword_t protocol_value)
{
	/* look up address and size of message payload */
	mword_t *msg_buf = (mword_t *)snd_msg.data();

	/* size of message payload in machine words */
	size_t const num_data_words = snd_msg.data_size()/sizeof(mword_t);

	/* account for protocol value in front of the message */
	size_t num_msg_words = 1 + num_data_words;

	enum { NUM_MSG_REGS = 256 };
	if (num_msg_words > NUM_MSG_REGS) {
		PERR("Message does not fit into UTCB message registers\n");
		num_msg_words = NUM_MSG_REGS;
	}

	utcb->msg[0] = protocol_value;

	/* store message into UTCB message registers */
	mword_t *src = (mword_t *)&msg_buf[0];
	mword_t *dst = (mword_t *)(void *)&utcb->msg[1];
	for (unsigned i = 0; i < num_data_words; i++)
		*dst++ = *src++;

	utcb->set_msg_word(num_msg_words);

	/* append portal capability selectors */
	for (unsigned i = 0; i < snd_msg.snd_pt_sel_cnt(); i++) {
		bool trans_map = true;
		Nova::Obj_crd crd = snd_msg.snd_pt_sel(i, trans_map);
		if (crd.base() == ~0UL) continue;

		if (!utcb->append_item(crd, i, false, false, trans_map))
			return false;
	}

	return true;
}


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t rcv_caps)
{
	/* update receive window for capability selectors if needed */
	if (rcv_caps != ~0UL) {

		/* calculate max order of caps to be received during reply */
		unsigned short log2_max = rcv_caps ? log2(rcv_caps) : 0;
		if ((1U << log2_max) < rcv_caps) log2_max ++;

		rcv_msg.rcv_wnd(log2_max);
	}

	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();

	/* the protocol value is unused as the badge is delivered by the kernel */
	if (!copy_msgbuf_to_utcb(utcb, snd_msg, 0)) {
		PERR("could not setup IPC");
		throw Ipc_error();
	}

	/* if we can't setup receive window, die in order to recognize the issue */
	if (!rcv_msg.prepare_rcv_window(utcb, dst.rcv_window()))
		/* printf doesn't work here since for IPC also rcv_prepare* is used */
		nova_die();

	/* establish the mapping via a portal traversal */
	uint8_t res = Nova::call(dst.local_name());
	if (res != Nova::NOVA_OK) {
		/* If an error occurred, reset word&item count (not done by kernel). */
		utcb->set_msg_word(0);
		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);
	}

	rcv_msg.post_ipc(utcb, dst.rcv_window());

	/* handle malformed reply from a server */
	if (utcb->msg_words() < 1)
		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);

	return Rpc_exception_code(copy_utcb_to_msgbuf(utcb, rcv_msg));
}


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::reply()
{
	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();

	copy_msgbuf_to_utcb(utcb, _snd_msg, _exception_code.value);

	_snd_msg.snd_reset();

	Nova::reply(Thread_base::myself()->stack_top());
}


void Ipc_server::reply_wait()
{
	/*
	 * This function is only called by the portal dispatcher of server
	 * entrypoint'. When the dispatcher is called, the incoming message already
	 * arrived so that we do not need to block. The only remaining thing to do
	 * is unmarshalling the arguments.
	 */

	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();

	_rcv_msg.post_ipc(utcb);

	/* handle ill-formed message */
	if (utcb->msg_words() < 2) {
		_rcv_msg.word(0) = ~0UL; /* invalid opcode */
	} else {
		copy_utcb_to_msgbuf(utcb, _rcv_msg);
	}

	_read_offset = _write_offset = 0;
}


Ipc_server::Ipc_server(Native_connection_state &cs,
                       Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg)
:
	Ipc_marshaller(snd_msg), Ipc_unmarshaller(rcv_msg), _rcv_cs(cs)
{
	_read_offset = _write_offset = 0;
}


Ipc_server::~Ipc_server() { }
