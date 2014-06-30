/*
 * \brief  Loader test program
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>
#include <loader_session/connection.h>
#include <timer_session/connection.h>


int main(int argc, char **argv)
{
	Loader::Connection loader(8*1024*1024);

	static Genode::Signal_receiver sig_rec;

	Genode::Signal_context sig_ctx;

	loader.view_ready_sigh(sig_rec.manage(&sig_ctx));

	loader.start("testnit", "test-label");

	sig_rec.wait_for_signal();

	Loader::Area size = loader.view_size();

	Timer::Connection timer;

	while (1) {

		for (unsigned i = 0; i < 10; i++) {

			loader.view_geometry(Loader::Rect(Loader::Point(50*i, 50*i), size),
			                     Loader::Point(0, 0));

			timer.msleep(1000);
		}
	}

	Genode::sleep_forever();

	return 0;
}
