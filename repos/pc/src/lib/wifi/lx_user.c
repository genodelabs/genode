/**
 * \brief  Lx user definitions
 * \author Josef Soentgen
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* DDE Linux includes */
#include <lx_user/io.h>

/* local includes */
#include "lx_user.h"


void lx_user_init(void)
{
	uplink_init();
	rfkill_init();
	socketcall_init();
}


void lx_user_handle_io(void) { }
