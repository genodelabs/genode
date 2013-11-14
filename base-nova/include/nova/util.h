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
	using namespace Nova;
	Utcb *utcb = (Utcb *)Genode::Thread_base::myself()->utcb();

	/* save original receive window */
	Crd orig_crd = utcb->crd_rcv;

	/* request event-handler portal */
	utcb->crd_rcv = Obj_crd(sel, log2_count);
	utcb->msg[0]  = event;
	utcb->msg[1]  = log2_count;
	utcb->set_msg_word(2);

	uint8_t res = call(cap.local_name());

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res)
		PERR("request of event (%lu) capability selector failed", event);
}


inline void request_native_ec_cap(Genode::Native_capability const &cap,
                                  Genode::addr_t sel) {
	request_event_portal(cap, sel , ~0UL, 1); }


inline void request_signal_sm_cap(Genode::Native_capability const &cap,
                                  Genode::addr_t sel) {
	request_event_portal(cap, sel, ~0UL - 1, 0); }


inline void delegate_vcpu_portals(Genode::Native_capability const &cap,
                                  Genode::addr_t sel)
{
	using namespace Nova;
	Utcb *utcb = reinterpret_cast<Utcb *>(Genode::Thread_base::myself()->utcb());

	/* save original receive window */
	Crd orig_crd = utcb->crd_rcv;

	utcb->crd_rcv = Obj_crd();
	utcb->set_msg_word(0);
	uint8_t res = utcb->append_item(Obj_crd(sel, NUM_INITIAL_VCPU_PT_LOG2), 0);
	(void)res;

	res = call(cap.local_name());

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;

	if (res)
		PERR("setting exception portals for vCPU failed %u", res);
}
#endif /* _NOVA__INCLUDE__UTIL_H_ */
