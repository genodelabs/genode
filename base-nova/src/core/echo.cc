/*
 * \brief  Echo implementation
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cap_sel_alloc.h>

/* Core includes */
#include <platform_pd.h>

/* local includes */
#include <echo.h>

enum {
	ECHO_UTCB_ADDR  = 0x50000000,
	ECHO_STACK_SIZE = 1024,
	ECHO_CPU_NO     = 0,
	ECHO_GLOBAL     = false,
	ECHO_EXC_BASE   = 0
};

inline void *echo_stack_top()
{
	static char echo_stack[ECHO_STACK_SIZE];
	return &echo_stack[ECHO_STACK_SIZE - sizeof(long)];
}


/**
 * IDC handler for the echo portal, executed by the echo EC
 */
static void echo_reply()
{
	/* collect map information from calling thread, sent as 3 words */
	Nova::Crd snd_rcv(echo()->utcb()->msg[0]);
	Nova::mword_t offset = echo()->utcb()->msg[1];
	bool kern_pd         = echo()->utcb()->msg[2];

	/* reset message transfer descriptor */
	echo()->utcb()->set_msg_word(0);
	/* append capability-range as message-transfer item */
	bool res = echo()->utcb()->append_item(snd_rcv, offset, kern_pd);

	/* set return code, 0 means failure */
	echo()->utcb()->msg[0] = res;
	echo()->utcb()->items += 1;

	/* during reply the mapping will be established */
	Nova::reply(echo_stack_top());
}


Echo::Echo(Genode::addr_t utcb_addr)
:
	_ec_sel(Genode::cap_selector_allocator()->alloc()),
	_pt_sel(Genode::cap_selector_allocator()->alloc()),
	_utcb((Nova::Utcb *)utcb_addr)
{
	using namespace Nova;

	/* create echo EC */
	Genode::addr_t pd_sel = Genode::Platform_pd::pd_core_sel();
	uint8_t res = create_ec(_ec_sel, pd_sel, ECHO_CPU_NO, utcb_addr,
	                        (mword_t)echo_stack_top(), ECHO_EXC_BASE, ECHO_GLOBAL);

	/* make error condition visible by raising an unhandled page fault */
	if (res) { ((void (*)())(res*0x10000UL))(); }

	/* set up echo portal to ourself */
	res = create_pt(_pt_sel, pd_sel, _ec_sel, Mtd(0), (mword_t)echo_reply);
	if (res) { ((void (*)())(res*0x10001UL))(); }

	/* echo thread doesn't receive anything, it transfers items during reply */
	utcb()->crd_rcv = utcb()->crd_xlt = 0;
}


Echo *echo() { static Echo inst(ECHO_UTCB_ADDR); return &inst; }
