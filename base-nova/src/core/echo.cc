/*
 * \brief  Echo implementation
 * \author Norman Feske
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
 */ static void echo_reply(){ Nova::reply(echo_stack_top()); }


Echo::Echo(Genode::addr_t utcb_addr)
:
	_ec_sel(Genode::cap_selector_allocator()->alloc()),
	_pt_sel(Genode::cap_selector_allocator()->alloc()),
	_utcb((Nova::Utcb *)utcb_addr)
{
	using namespace Nova;

	/* create echo EC */
	int pd_sel = Genode::Cap_selector_allocator::pd_sel();
	uint8_t res = create_ec(_ec_sel, pd_sel, ECHO_CPU_NO, utcb_addr,
	                        (mword_t)echo_stack_top(), ECHO_EXC_BASE, ECHO_GLOBAL);

	/* make error condition visible by raising an unhandled page fault */
	if (res) { ((void (*)())(res*0x10000UL))(); }

	/* set up echo portal to ourself */
	res = create_pt(_pt_sel, pd_sel, _ec_sel, Mtd(0), (mword_t)echo_reply);
	if (res) { ((void (*)())(res*0x10001UL))(); }
}


Echo *echo() { static Echo inst(ECHO_UTCB_ADDR); return &inst; }
