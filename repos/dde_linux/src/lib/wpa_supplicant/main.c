/*
 * \brief  WPA Supplicant frontend
 * \author Josef Soentgen
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * based on:
 *
 * WPA Supplicant / Example program entrypoint
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

/* libc includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* local includes */
#include "includes.h"
#include "common.h"
#include "eloop.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "scan.h"


int wpa_main(void)
{
	struct wpa_interface iface;
	int exitcode = 0;
	struct wpa_params params;
	struct wpa_global *global;

	memset(&params, 0, sizeof(params));

	// TODO use CTRL interface for setting debug level
	params.wpa_debug_level = 1 ? MSG_DEBUG : MSG_INFO;
	params.ctrl_interface = "GENODE";

	global = wpa_supplicant_init(&params);
	if (global == NULL)
		return -1;

	memset(&iface, 0, sizeof(iface));

	iface.ifname   = "wlan0";
	iface.confname = 0;
	iface.ctrl_interface = "GENODE";

	if (wpa_supplicant_add_iface(global, &iface, NULL) == NULL)
		exitcode = -1;

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

	return exitcode;
}
