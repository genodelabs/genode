/*
 * \brief  IPC utility functions
 * \author Norman Feske
 * \date   2016-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__IPC_H_
#define _INCLUDE__BASE__INTERNAL__IPC_H_

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/native_thread.h>
#include <nova/capability_space.h>

/**
 * Copy message registers from UTCB to destination message buffer
 *
 * \return  protocol word delivered via the first UTCB message register
 *
 * The caller of this function must ensure that utcb.msg_words is greater
 * than 0.
 */
static inline Nova::mword_t copy_utcb_to_msgbuf(Nova::Utcb             &utcb,
                                                Genode::Receive_window &rcv_window,
                                                Genode::Msgbuf_base    &rcv_msg)
{
	using namespace Genode;
	using namespace Nova;

	size_t num_msg_words = utcb.msg_words();

	/*
	 * Handle the reception of a malformed message. This should never happen
	 * because the utcb.msg_words is checked by the caller of this function.
	 */
	if (num_msg_words < 1)
		return 0;

	/* the UTCB contains the protocol word followed by the message data */
	mword_t const protocol_word  = utcb.msg[0];
	size_t        num_data_words = num_msg_words - 1;

	if (num_data_words*sizeof(mword_t) > rcv_msg.capacity()) {
		error("receive message buffer too small msg "
		      "size=", num_data_words*sizeof(mword_t), " "
		      "buf size=", rcv_msg.capacity());
		num_data_words = rcv_msg.capacity()/sizeof(mword_t);
	}

	/* read message payload into destination message buffer */
	mword_t *src = (mword_t *)(void *)(&utcb.msg[1]);
	mword_t *dst = (mword_t *)rcv_msg.data();
	for (unsigned i = 0; i < num_data_words; i++)
		*dst++ = *src++;

	/* extract caps from UTCB */
	for (unsigned i = 0; i < rcv_window.num_received_caps(); i++) {
		Native_capability cap;
		rcv_window.rcv_pt_sel(cap);
		rcv_msg.insert(cap);
	}

	return protocol_word;
}


/**
 * Copy message payload to UTCB message registers
 */
static inline bool copy_msgbuf_to_utcb(Nova::Utcb                &utcb,
                                       Genode::Msgbuf_base const &snd_msg,
                                       Nova::mword_t              protocol_value)
{
	using namespace Genode;
	using namespace Nova;

	/* look up address and size of message payload */
	mword_t *msg_buf = (mword_t *)snd_msg.data();

	/* size of message payload in machine words */
	size_t const num_data_words = snd_msg.data_size()/sizeof(mword_t);

	/* account for protocol value in front of the message */
	size_t num_msg_words = 1 + num_data_words;

	enum { NUM_MSG_REGS = 256 };
	if (num_msg_words > NUM_MSG_REGS) {
		error("message does not fit into UTCB message registers");
		num_msg_words = NUM_MSG_REGS;
	}

	utcb.msg[0] = protocol_value;

	/* store message into UTCB message registers */
	mword_t *src = (mword_t *)&msg_buf[0];
	mword_t *dst = (mword_t *)(void *)&utcb.msg[1];
	for (unsigned i = 0; i < num_data_words; i++)
		*dst++ = *src++;

	utcb.set_msg_word(num_msg_words);

	/* append portal capability selectors */
	for (unsigned i = 0; i < snd_msg.used_caps(); i++) {

		Native_capability const &cap = snd_msg.cap(i);
		Nova::Crd const crd = Capability_space::crd(cap);

		if (crd.base() == ~0UL) continue;

		if (!utcb.append_item(crd, i, false, false, true))
			return false;
	}

	return true;
}


#endif /* _INCLUDE__BASE__INTERNAL__IPC_H_ */
