/*
 * \brief  lwip nic interface initialisation using DHCP
 * \author Christian Prochaska
 * \date   2010-04-29
 *
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <parent/parent.h>

#include <lwip/genode.h>

extern void create_lwip_plugin();

void __attribute__((constructor)) init_nic_dhcp(void)
{
	PDBG("init_nic_dhcp()\n");

	/* make sure the libc_lwip plugin has been created */
	create_lwip_plugin();

	try {
		lwip_nic_init(0, 0, 0);
	} catch (Genode::Parent::Service_denied) {
		/* ignore for now */
	}
}
