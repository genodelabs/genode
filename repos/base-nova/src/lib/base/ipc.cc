/*
 * \brief  Implementation of the IPC API for NOVA
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/thread.h>
#include <base/log.h>

/* base-internal includes */
#include <base/internal/ipc.h>

/* NOVA includes */
#include <nova/cap_map.h>

using namespace Genode;


/****************
 ** IPC client **
 ****************/

Rpc_exception_code Genode::ipc_call(Native_capability dst,
                                    Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
                                    size_t rcv_caps)
{
	Receive_window rcv_window;
	rcv_msg.reset();

	/* update receive window for capability selectors if needed */
	if (rcv_caps != ~0UL) {

		/* calculate max order of caps to be received during reply */
		unsigned short log2_max = 0;
		if (rcv_caps) {
			log2_max = log2(rcv_caps);

			/* if this happens, the call is bogus and invalid */
			if ((log2_max >= sizeof(rcv_caps) * 8))
				throw Ipc_error();

			if ((1UL << log2_max) < rcv_caps)
				log2_max ++;
		}

		rcv_window.rcv_wnd(log2_max);
	}

	Thread * const myself = Thread::myself();
	Nova::Utcb &utcb = *(Nova::Utcb *)myself->utcb();

	/* the protocol value is unused as the badge is delivered by the kernel */
	if (!copy_msgbuf_to_utcb(utcb, snd_msg, 0)) {
		error("could not setup IPC");
		throw Ipc_error();
	}

	/*
	 * Determine manually defined selector for receiving the call result.
	 * See the comment in 'base-nova/include/nova/native_thread.h'.
	 */
	addr_t const manual_rcv_sel = myself ? myself->native_thread().client_rcv_sel
	                                     : Receive_window::INVALID_INDEX;

	/* if we can't setup receive window, die in order to recognize the issue */
	if (!rcv_window.prepare_rcv_window(utcb, manual_rcv_sel))
		/* printf doesn't work here since for IPC also rcv_prepare* is used */
		nova_die();

	/* establish the mapping via a portal traversal */
	uint8_t res = Nova::call(dst.local_name());

	if (res != Nova::NOVA_OK)
		/* If an error occurred, reset word&item count (not done by kernel). */
		utcb.set_msg_word(0);

	/* track potentially received caps and invalidate unused caps slots */
	rcv_window.post_ipc(utcb, manual_rcv_sel);

	if (res != Nova::NOVA_OK)
		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);

	/* handle malformed reply from a server */
	if (utcb.msg_words() < 1)
		return Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);

	return Rpc_exception_code(copy_utcb_to_msgbuf(utcb, rcv_window, rcv_msg));
}


/********************
 ** Receive_window **
 ********************/

void Receive_window::rcv_pt_sel(Native_capability &cap)
{
	if (_rcv_pt_sel_cnt >= _rcv_pt_sel_max) {
		cap = Native_capability();
		return;
	}

	/* return only received or translated caps */
	cap = Capability_space::import(_rcv_pt_sel[_rcv_pt_sel_cnt++].sel);
}


bool Receive_window::rcv_invalid() const
{
	return _rcv_pt_base == Capability_space::INVALID_INDEX;
}


bool Receive_window::rcv_cleanup(bool keep, unsigned short const new_max)
{
	/* mark used mapped capabilities as used to prevent freeing */
	bool reinit = false;
	for (unsigned i = 0; i < _rcv_pt_sel_cnt; i++) {
		if (!_rcv_pt_sel[i].del)
			continue;

		/* should never happen */
		if (_rcv_pt_sel[i].sel < _rcv_pt_base ||
			(_rcv_pt_sel[i].sel >= _rcv_pt_base + MAX_CAP_ARGS))
				nova_die();

		_rcv_pt_cap_free [_rcv_pt_sel[i].sel - _rcv_pt_base] = USED_CAP;

		reinit = true;
	}

	/* if old receive window was smaller, we need to re-init */
	for (unsigned i = 0; !reinit && i < new_max; i++)
		if (_rcv_pt_cap_free[i] == FREE_INVALID)
			reinit = true;

	_rcv_pt_sel_cnt = 0;
	_rcv_pt_sel_max = 0;

	/* we can keep the cap selectors if none was used */
	if (keep && !reinit) {
		for (unsigned i = 0; i < MAX_CAP_ARGS; i++) {
			/* revoke received caps which are unused */
			if (_rcv_pt_cap_free[i] == UNUSED_CAP)
				Nova::revoke(Nova::Obj_crd(_rcv_pt_base + i, 0), true);

			/* free rest of indexes if new_max is smaller then last window */
			if (i >= new_max && _rcv_pt_cap_free[i] == FREE_SEL)
				cap_map().remove(_rcv_pt_base + i, 0, false);
		}

		return false;
	}

	/* decrease ref count if valid selector */
	for (unsigned i = 0; i < MAX_CAP_ARGS; i++) {
		if (_rcv_pt_cap_free[i] == FREE_INVALID)
			continue;
		cap_map().remove(_rcv_pt_base + i, 0, _rcv_pt_cap_free[i] != FREE_SEL);
	}

	return true;
}


bool Receive_window::prepare_rcv_window(Nova::Utcb &utcb, addr_t rcv_window)
{
	/* open maximal translate window */
	utcb.crd_xlt = Nova::Obj_crd(0, ~0UL);

	/* use receive window if specified */
	if (rcv_window != INVALID_INDEX) {
		/* cleanup if receive window already used */
		if (!rcv_invalid()) rcv_cleanup(false);

		_rcv_pt_base = rcv_window;

		/* open receive window */
		utcb.crd_rcv = Nova::Obj_crd(_rcv_pt_base, _rcv_wnd_log2);
		return true;
	}

	/* allocate receive window if necessary, otherwise use old one */
	if (rcv_invalid() || rcv_cleanup(true, 1U << _rcv_wnd_log2))
	{
		_rcv_pt_base = cap_map().insert(_rcv_wnd_log2);

		if (_rcv_pt_base == INVALID_INDEX) {
			/* no mappings can be received */
			utcb.crd_rcv = Nova::Obj_crd();
			return false;
		}
	}

	/* open receive window */
	utcb.crd_rcv = Nova::Obj_crd(_rcv_pt_base, _rcv_wnd_log2);
	return true;
}
