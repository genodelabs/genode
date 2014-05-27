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

#ifndef _NOVA__INCLUDE__UTIL_H_
#define _NOVA__INCLUDE__UTIL_H_

#include <base/printf.h>
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
	Genode::Thread_base * myself = Genode::Thread_base::myself();
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
		PERR("request of event (%lu) capability selector failed", event);
}


inline void request_native_ec_cap(Genode::Native_capability const &cap,
                                  Genode::addr_t sel, unsigned pager_cap = 1) {
	request_event_portal(cap, sel , ~0UL, pager_cap); }


inline void request_signal_sm_cap(Genode::Native_capability const &cap,
                                  Genode::addr_t sel) {
	request_event_portal(cap, sel, ~0UL - 1, 0); }


inline void delegate_vcpu_portals(Genode::Native_capability const &cap,
                                  Genode::addr_t sel)
{
	Genode::Thread_base * myself = Genode::Thread_base::myself();
	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	/* save original receive window */
	Nova::Crd orig_crd = utcb->crd_rcv;

	Nova::Obj_crd obj_crd(sel, Nova::NUM_INITIAL_VCPU_PT_LOG2);

	utcb->crd_rcv = Nova::Obj_crd();
	utcb->set_msg_word(0);
	Genode::uint8_t res = utcb->append_item(obj_crd, 0);
	(void)res;

	res = Nova::call(cap.local_name());

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res)
		PERR("setting exception portals for vCPU failed %u", res);
}
#endif /* _NOVA__INCLUDE__UTIL_H_ */
