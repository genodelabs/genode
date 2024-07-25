/*
 * \brief  Helper code used by core as base framework
 * \author Alexander Boettcher
 * \date   2012-08-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA__UTIL_H_
#define _INCLUDE__NOVA__UTIL_H_

#include <base/log.h>
#include <base/thread.h>

__attribute__((always_inline))
inline void nova_die()
{
	asm volatile ("ud2a");
}


inline void request_event_portal(Genode::addr_t const cap,
                                 Genode::addr_t const sel, Genode::addr_t event)
{
	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	/* save original receive window */
	Nova::Crd orig_crd = utcb->crd_rcv;

	/* request event-handler portal */
	utcb->crd_rcv = Nova::Obj_crd(sel, 0);
	utcb->msg()[0]  = event;
	utcb->set_msg_word(1);

	Genode::uint8_t res = Nova::call(cap);

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res)
		Genode::error("request of event (", Genode::Hex(event), ") ",
		              "capability selector failed (res=", res, ")");
}


inline void request_native_ec_cap(Genode::addr_t const cap,
                                  Genode::addr_t const sel)
{
	request_event_portal(cap, sel , ~0UL);
}


inline void request_signal_sm_cap(Genode::addr_t const cap,
                                  Genode::addr_t const sel)
{
	request_event_portal(cap, sel, ~0UL - 1);
}


inline void translate_remote_pager(Genode::addr_t const cap,
                                   Genode::addr_t const sel)
{
	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	/* save original receive window */
	Nova::Crd orig_crd = utcb->crd_rcv;

	utcb->crd_rcv = Nova::Obj_crd();

	Genode::uint8_t res = Nova::NOVA_OK;
	enum {
		TRANSLATE = true, THIS_PD = false, NON_GUEST = false, HOTSPOT = 0
	};

	/* translate one item */
	utcb->msg()[0] = 0xaffe;
	utcb->set_msg_word(1);

	Nova::Obj_crd obj_crd(sel, 0);
	if (utcb->append_item(obj_crd, HOTSPOT, THIS_PD, NON_GUEST, TRANSLATE))
		/* trigger the translation */
		res = Nova::call(cap);

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res != Nova::NOVA_OK)
		Genode::error("setting exception portals for vCPU failed res=", res);
}
#endif /* _INCLUDE__NOVA__UTIL_H_ */
