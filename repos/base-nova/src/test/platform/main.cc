/*
 * \brief  Some platform tests for the base-nova
 * \author Alexander Boettcher
 * \date   2015-01-02
 *
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <base/printf.h>
#include <base/snprintf.h>

using namespace Genode;

static unsigned failed = 0;

void check(uint8_t res, const char *format, ...)
{
	static char buf[128];

	va_list list;
	va_start(list, format);

	String_console sc(buf, sizeof(buf));
	sc.vprintf(format, list);

	va_end(list);

	if (res == Nova::NOVA_OK) {
		PERR("res=%u %s - TEST FAILED", res, buf);
		failed++;
	}
	else
		printf("res=%u %s\n", res, buf);
}

int main(int argc, char **argv)
{
	printf("testing base-nova platform\n");

	Thread_base * myself = Thread_base::myself();
	if (!myself)
		return -__LINE__;

	addr_t sel_pd  = cap_map()->insert();
	addr_t sel_ec  = myself->tid().ec_sel;
	addr_t sel_cap = cap_map()->insert();
	addr_t handler = 0UL;
	uint8_t    res = 0;

	Nova::Mtd mtd(Nova::Mtd::ALL);

	if (sel_cap == ~0UL || sel_ec == ~0UL || sel_cap == ~0UL)
		return -__LINE__;

	/* negative syscall tests - they should not succeed */
	res = Nova::create_pt(sel_cap, sel_pd, sel_ec, mtd, handler);
	check(res, "create_pt");

	res = Nova::create_sm(sel_cap, sel_pd, 0);
	check(res, "create_sm");

	/* changing the badge of one of the portal must fail */
	for (unsigned i = 0; i < (1U << Nova::NUM_INITIAL_PT_LOG2); i++) {
		addr_t sel_exc = myself->tid().exc_pt_sel + i;
		res = Nova::pt_ctrl(sel_exc, 0xbadbad);
		check(res, "pt_ctrl %2u", i);
	}

	if (!failed)
		printf("Test finished\n");

	return -failed;
}
