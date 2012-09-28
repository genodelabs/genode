/*
 * \brief  Helper code used by core as base framework
 * \author Alexander Boettcher
 * \date   2012-08-08
 */

/*
 * Copyright (C) 2012-2012 Genode Labs GmbH
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

inline void request_event_portal(Genode::Native_capability cap,
                                 Genode::addr_t exc_base, Genode::addr_t event)
{
	using namespace Nova;
	Utcb *utcb = (Utcb *)Genode::Thread_base::myself()->utcb();

	/* save original receive window */
	Crd orig_crd = utcb->crd_rcv;

	/* request event-handler portal */
	utcb->crd_rcv = Obj_crd(exc_base + event, 0);
	utcb->msg[0]  = event;
	utcb->set_msg_word(1);

	uint8_t res = call(cap.local_name());
	if (res)
		PERR("request of event (%lu) capability selector failed",
		     event);

	/* restore original receive window */
	utcb->crd_rcv = orig_crd;
}


#endif /* _NOVA__INCLUDE__UTIL_H_ */
