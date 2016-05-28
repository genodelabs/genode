/*
 * \brief  WPA Supplicant frontend
 * \author Josef Soentgen
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
#include "wpa_supplicant_i.h"


static char const *conf_file = "/config/wpa_supplicant.conf";


int wpa_main(int debug_msg)
{
	struct wpa_interface iface;
	int exitcode = 0;
	struct wpa_params params;
	struct wpa_global *global;

	memset(&params, 0, sizeof(params));

	params.wpa_debug_level = debug_msg ? MSG_DEBUG : MSG_INFO;

	global = wpa_supplicant_init(&params);
	if (global == NULL)
		return -1;

	memset(&iface, 0, sizeof(iface));

	iface.ifname   = "wlan0";
	iface.confname = conf_file;

	if (wpa_supplicant_add_iface(global, &iface) == NULL)
		exitcode = -1;

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

	return exitcode;
}


void eloop_handle_signal(int);
void wpa_conf_reload(void)
{
	/* (ab)use POSIX signal to trigger reloading the conf file */
	eloop_handle_signal(SIGHUP);
}


int wpa_write_conf(char const *buffer, size_t len)
{
	int fd = open(conf_file, O_TRUNC|O_WRONLY);
	if (fd == -1)
		return -1;

	ssize_t n = write(fd, buffer, len);
	close(fd);

	return n > 0 ? 0 : -1;
}
