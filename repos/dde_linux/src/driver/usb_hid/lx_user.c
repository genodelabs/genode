/*
 * \brief  Post kernel activity
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/types.h>
#include <lx_emul/input_leds.h>
#include <lx_emul/usb_client.h>
#include <lx_user/init.h>
#include <lx_user/io.h>

void lx_user_init(void)
{
	lx_emul_usb_client_init();
	lx_emul_input_leds_init();
}


void lx_user_handle_io(void)
{
	lx_emul_usb_client_ticker();
}
