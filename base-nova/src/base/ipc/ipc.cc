/*
 * \brief  Implementation of the IPC API for NOVA
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>

#include <base/printf.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;
using namespace Nova;


/***************
 ** Utilities **
 ***************/

/**
 * Copy message registers from UTCB to destination message buffer
 */
static void copy_utcb_to_msgbuf(Nova::Utcb *utcb, Msgbuf_base *rcv_msg)
{
	size_t num_msg_words = utcb->msg_words();
	if (num_msg_words == 0) return;

	/* look up and validate destination message buffer to receive the payload */
	mword_t *msg_buf = (mword_t *)rcv_msg->buf;
	if (num_msg_words*sizeof(mword_t) > rcv_msg->size()) {
		PERR("receive message buffer too small msg size=%zx, buf size=%zd",
		     num_msg_words*sizeof(mword_t), rcv_msg->size());
		num_msg_words = rcv_msg->size()/sizeof(mword_t);
	}

	/* read message payload into destination message buffer */
	mword_t *src = (mword_t *)(void *)(&utcb->msg[0]);
	mword_t *dst = (mword_t *)&msg_buf[0];
	for (unsigned i = 0; i < num_msg_words; i++)
		*dst++ = *src++;

	rcv_msg->rcv_reset();
}


/**
 * Copy message payload to UTCB message registers
 */
static void copy_msgbuf_to_utcb(Nova::Utcb *utcb, Msgbuf_base *snd_msg,
                                unsigned num_msg_words, mword_t local_name)
{
	/* look up address and size of message payload */
	mword_t *msg_buf = (mword_t *)snd_msg->buf;

	/*
	 * XXX determine correct number of message registers
	 */
	enum { NUM_MSG_REGS = 256 };
	if (num_msg_words > NUM_MSG_REGS) {
		PERR("Message does not fit into UTCB message registers\n");
		num_msg_words = NUM_MSG_REGS;
	}

	msg_buf[0] = local_name;

	/* store message into UTCB message registers */
	mword_t *src = (mword_t *)&msg_buf[0];
	mword_t *dst = (mword_t *)(void *)&utcb->msg[0];
	for (unsigned i = 0; i < num_msg_words; i++)
		*dst++ = *src++;

	utcb->set_msg_word(num_msg_words);

	/* append portal capability selectors */
	for (unsigned i = 0; i < snd_msg->snd_pt_sel_cnt(); i++) {
		int pt_sel = snd_msg->snd_pt_sel(i);
		if (pt_sel < 0) continue;

		utcb->append_item(Nova::Obj_crd(pt_sel, 0), i);
	}

	/* we have consumed portal capability selectors, reset message buffer */
	snd_msg->snd_reset();
}


/*****************
 ** Ipc_ostream **
 *****************/

Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{
	_write_offset = sizeof(mword_t);
}


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait() { }


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg)
: Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()), _rcv_msg(rcv_msg)
{
	_read_offset = sizeof(mword_t);
}


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call()
{
	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();

	copy_msgbuf_to_utcb(utcb, _snd_msg, _write_offset/sizeof(mword_t),
	                    Ipc_ostream::_dst.local_name());
	_rcv_msg->rcv_prepare_pt_sel_window(utcb);

	/* establish the mapping via a portal traversal */
	if (Ipc_ostream::_dst.dst() == 0)
		PWRN("destination portal is zero");
	int res = Nova::call(Ipc_ostream::_dst.dst());
	if (res)
		PERR("call returned %d", res);

	copy_utcb_to_msgbuf(utcb, _rcv_msg);
	_snd_msg->snd_reset();

	_write_offset = _read_offset = sizeof(mword_t);
}


Ipc_client::Ipc_client(Native_capability const &srv,
                       Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{ }


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_wait()
{
	/*
	 * This function is only called by the portal dispatcher of server
	 * entrypoint'. When the dispatcher is called, the incoming message already
	 * arrived so that we do not need to block. The only remaining thing to do
	 * is unmarshalling the arguments.
	 */

	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();

	copy_utcb_to_msgbuf(utcb, _rcv_msg);

	/* reset unmarshaller */
	_read_offset  = sizeof(mword_t);
	_write_offset = 2*sizeof(mword_t); /* leave space for the return value */
}


void Ipc_server::_reply()
{
	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();

	copy_msgbuf_to_utcb(utcb, _snd_msg, _write_offset/sizeof(mword_t),
	                    Ipc_ostream::_dst.local_name());

	Nova::reply(Thread_base::myself()->stack_top());
}


void Ipc_server::_reply_wait() { }


Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(Native_capability(), snd_msg)
{ }
