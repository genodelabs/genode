/*
 * \brief  Input service test program
 * \author Christian Helmuth
 * \date   2010-06-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <input_session/connection.h>
#include <input/event.h>

#include "key_strings.h"

using namespace Genode;

int main(int argc, char **argv)
{
	/*
	 * Init sessions to the required external services
	 */
	static Input::Connection input;
	static Timer::Connection timer;

	PLOG("--- Input test is up ---");

	Input::Event *ev_buf = (env()->rm_session()->attach(input.dataspace()));

	PLOG("input buffer at %p", ev_buf);

	/*
	 * Handle input events
	 */
	int key_cnt = 0;
	while (1) {
		/* poll input service every 20 ms */
		while (!input.is_pending()) timer.msleep(20);

		for (int i = 0, num_ev = input.flush(); i < num_ev; ++i) {

			Input::Event *ev = &ev_buf[i];

			if (ev->type() == Input::Event::PRESS)   key_cnt++;
			if (ev->type() == Input::Event::RELEASE) key_cnt--;

			/* log event */
			PLOG("Input event type=%d\tcode=%d\trx=%d\try=%d\tkey_cnt=%d\t%s",
			     ev->type(), ev->code(), ev->rx(), ev->ry(), key_cnt,
			     key_strings[ev->code()]);
		}
	}

	return 0;
}
