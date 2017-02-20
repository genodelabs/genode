/*
 * \brief  Echo implementation
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <platform.h>

/* local includes */
#include <echo.h>
#include <nova_util.h>

enum {
	ECHO_STACK_SIZE = 512,
	ECHO_GLOBAL     = false,
	ECHO_EXC_BASE   = 0,
	ECHO_LOG2_COUNT = 1 /* selector for EC and out-of-memory portal */
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
	bool dma_mem         = echo()->utcb()->msg[3];
	bool write_combined  = echo()->utcb()->msg[4];

	/* reset message transfer descriptor */
	echo()->utcb()->set_msg_word(0);
	/* append capability-range as message-transfer item */
	bool res = echo()->utcb()->append_item(snd_rcv, offset, kern_pd, false,
	                                       false, dma_mem, write_combined);

	/* set return code, 0 means failure */
	echo()->utcb()->msg[0] = res;
	echo()->utcb()->items += 1;

	/* during reply the mapping will be established */
	Nova::reply(echo_stack_top());
}


Echo::Echo(Genode::addr_t utcb_addr)
:
	_ec_sel(Genode::cap_map()->insert(ECHO_LOG2_COUNT)),
	_pt_sel(Genode::cap_map()->insert()),
	_utcb((Nova::Utcb *)utcb_addr)
{
	using namespace Nova;

	extern Genode::addr_t __initial_sp;
	Hip const * const hip = reinterpret_cast<Hip *>(__initial_sp);

	/* create echo EC */
	Genode::addr_t const core_pd_sel = hip->sel_exc;
	uint8_t res = create_ec(_ec_sel, core_pd_sel, boot_cpu(), utcb_addr,
	                        reinterpret_cast<mword_t>(echo_stack_top()),
	                        ECHO_EXC_BASE, ECHO_GLOBAL);

	/* make error condition visible by raising an unhandled page fault */
	if (res != Nova::NOVA_OK) { *reinterpret_cast<unsigned *>(0) = 0xdead; }

	/* set up echo portal to ourself */
	res = create_pt(_pt_sel, core_pd_sel, _ec_sel, Mtd(0), (mword_t)echo_reply);
	if (res != Nova::NOVA_OK) { *reinterpret_cast<unsigned *>(0) = 0xdead; }
	revoke(Obj_crd(_pt_sel, 0, Obj_crd::RIGHT_PT_CTRL));

	/* echo thread doesn't receive anything, it transfers items during reply */
	utcb()->crd_rcv = utcb()->crd_xlt = 0;
}


Echo *echo() { static Echo inst(Echo::ECHO_UTCB_ADDR); return &inst; }
