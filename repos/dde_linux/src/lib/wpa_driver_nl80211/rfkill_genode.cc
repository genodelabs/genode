/*
 * \brief  RFKILL backend
 * \author Josef Soentgen
 * \date   2018-07-11
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * based on:
 *
 * Linux rfkill helper functions for driver wrappers
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

/* rep includes */
#include <wifi/rfkill.h>

extern "C" {
#include "includes.h"

#include "utils/common.h"
#include "utils/eloop.h"

#include <drivers/rfkill.h>
} /* extern "C" */


struct rfkill_data {
	struct rfkill_config *cfg;
	int fd;
	bool blocked;
};


static void rfkill_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct rfkill_data * const rfkill = (rfkill_data*)eloop_ctx;
	bool const new_blocked = wifi_get_rfkill();

	if (new_blocked != rfkill->blocked) {
		rfkill->blocked = new_blocked;

		if (new_blocked) {
			rfkill->cfg->blocked_cb(rfkill->cfg->ctx);
		} else {
			rfkill->cfg->unblocked_cb(rfkill->cfg->ctx);
		}
	}
}


struct rfkill_data * rfkill_init(struct rfkill_config *cfg)
{
	struct rfkill_data *rfkill = (rfkill_data*) os_zalloc(sizeof(*rfkill));
	if (!rfkill) { return NULL; }

	rfkill->cfg = cfg;
	rfkill->fd  = Wifi::RFKILL_FD;

	eloop_register_read_sock(rfkill->fd, rfkill_receive, rfkill, NULL);

	return rfkill;
}


void rfkill_deinit(struct rfkill_data *rfkill)
{
	if (!rfkill) { return; }

	eloop_unregister_read_sock(rfkill->fd);

	os_free(rfkill->cfg);
	os_free(rfkill);
}


int rfkill_is_blocked(struct rfkill_data *rfkill)
{
	return rfkill ? rfkill->blocked : 0;
}
