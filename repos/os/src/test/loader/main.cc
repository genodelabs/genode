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
#include <nitpicker_view/client.h>
#include <timer_session/connection.h>

using namespace Genode;

int main(int argc, char **argv)
{
	Loader::Connection loader(8*1024*1024);

	static Signal_receiver sig_rec;

	Signal_context sig_ctx;

	loader.view_ready_sigh(sig_rec.manage(&sig_ctx));

	loader.start("testnit", "test-label");

	sig_rec.wait_for_signal();

	Loader::Session::View_geometry geometry = loader.view_geometry();
	Nitpicker::View_capability view_cap = loader.view();

	Nitpicker::View_client view(view_cap);
	view.stack(Nitpicker::View_capability(), true, false);

	Timer::Connection timer;

	while(1) {

		for (unsigned i = 0; i < 10; i++) {
			view.viewport(50*i, 50*i, geometry.width, geometry.height,
			              geometry.buf_x, geometry.buf_y, true);
			timer.msleep(1000);
		}
	}

	sleep_forever();

	return 0;
}
