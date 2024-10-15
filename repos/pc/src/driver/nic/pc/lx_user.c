/*
 * \brief  PC Ethernet driver
 * \author Christian Helmuth
 * \date   2023-05-25
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_user/init.h>
#include <lx_emul/nic.h>


void lx_user_init(void)
{
	lx_emul_nic_init();
}


void lx_user_handle_io(void)
{
	lx_emul_nic_handle_io();
}
