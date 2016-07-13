/*
 * \brief  Helper code used by core as base framework
 * \author Alexander Boettcher
 * \date   2012-08-08
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOVA__UTIL_H_
#define _INCLUDE__NOVA__UTIL_H_

#include <base/log.h>
#include <base/thread.h>

__attribute__((always_inline))
inline void nova_die(const char * text = 0)
{
	/*
	 * If thread is de-constructed the sessions are already gone.
	 * Be careful when enabling printf here.
	 */
	while (1)
		asm volatile ("ud2a" : : "a"(text));
}


inline void request_event_portal(Genode::Native_capability const &cap,
                                 Genode::addr_t sel, Genode::addr_t event,
                                 unsigned short log2_count = 0)
{
	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	/* save original receive window */
	Nova::Crd orig_crd = utcb->crd_rcv;

	/* request event-handler portal */
	utcb->crd_rcv = Nova::Obj_crd(sel, log2_count);
	utcb->msg[0]  = event;
	utcb->msg[1]  = log2_count;
	utcb->set_msg_word(2);

	Genode::uint8_t res = Nova::call(cap.local_name());

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res)
		Genode::error("request of event (", event, ") ",
		              "capability selector failed (res=", res, ")");
}


inline void request_native_ec_cap(Genode::Native_capability const &cap,
                                  Genode::addr_t const sel,
                                  unsigned const no_pager_cap = 0)
{
	request_event_portal(cap, sel , ~0UL, no_pager_cap);
}


inline void request_signal_sm_cap(Genode::Native_capability const &cap,
                                  Genode::addr_t const sel)
{
	request_event_portal(cap, sel, ~0UL - 1, 0);
}


inline void delegate_vcpu_portals(Genode::Native_capability const &cap,
                                  Genode::addr_t const sel)
{
	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	/* save original receive window */
	Nova::Crd orig_crd = utcb->crd_rcv;

	utcb->crd_rcv = Nova::Obj_crd();

	Genode::uint8_t res = Nova::NOVA_OK;
	enum {
		TRANSLATE = true, THIS_PD = false, NON_GUEST = false, HOTSPOT = 0,
		TRANSFER_ITEMS = 1U << (Nova::NUM_INITIAL_VCPU_PT_LOG2 - 1)
	};

	/* prepare translation items for every portal separately */
	for (unsigned half = 0; !res && half < 2; half++) {
		/* translate half of portals - due to size constraints on 64bit */
		utcb->msg[0] = half;
		utcb->set_msg_word(1);
		/* add one translate item per portal */
		for (unsigned i = 0; !res && i < TRANSFER_ITEMS; i++) {
			Nova::Obj_crd obj_crd(sel + half * TRANSFER_ITEMS + i, 0);

			if (!utcb->append_item(obj_crd, HOTSPOT, THIS_PD, NON_GUEST,
			                       TRANSLATE))
				res = 0xff;
		}
		if (res != Nova::NOVA_OK)
			break;

		/* trigger the translation */
		res = Nova::call(cap.local_name());
	}

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res)
		Genode::error("setting exception portals for vCPU failed res=", res);
}
#endif /* _INCLUDE__NOVA__UTIL_H_ */
