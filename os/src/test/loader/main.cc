/*
 * \brief  Loader test program
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <loader_session/connection.h>
#include <nitpicker_view/client.h>
#include <rom_session/connection.h>
#include <timer_session/connection.h>
#include <util/arg_string.h>

using namespace Genode;

int main(int argc, char **argv)
{
	Rom_connection rc("testnit");
	Dataspace_capability rom_ds = rc.dataspace();
	char *rom_ds_addr = (char*)env()->rm_session()->attach(rom_ds);

	size_t rom_ds_size = Dataspace_client(rom_ds).size();
	char args[256] = "";
	Arg_string::set_arg(args, sizeof(args),"ram_quota", "8M");
	Arg_string::set_arg(args, sizeof(args),"ds_size", rom_ds_size);

	Loader::Connection lc(args);
	Dataspace_capability loader_ds = lc.dataspace();
	char *loader_ds_addr = (char*)env()->rm_session()->attach(loader_ds);

	memcpy(loader_ds_addr, rom_ds_addr, rom_ds_size);

	env()->rm_session()->detach(loader_ds_addr);
	env()->rm_session()->detach(rom_ds_addr);

	lc.start("ram_quota=4M", 800, 600, 1000, "testnit");

	int w, h, buf_x, buf_y;
	Nitpicker::View_capability view_cap = lc.view(&w, &h, &buf_x, &buf_y);

	PDBG("w = %d, h = %d, buf_x = %d, buf_y = %d", w, h, buf_x, buf_y);

	Nitpicker::View_client vc(view_cap);
	vc.stack(Nitpicker::View_capability(), true, false);

	Timer::Connection tc;

	while(1) {
		vc.viewport(0, 0, w, h, buf_x, buf_y, true);
		tc.msleep(1000);
		vc.viewport(50, 50, w, h, buf_x, buf_y, true);
		tc.msleep(1000);
	}

	sleep_forever();

	return 0;
}
