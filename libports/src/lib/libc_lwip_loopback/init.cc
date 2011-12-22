/*
 * \brief  lwip loopback interface initialization
 * \author Christian Prochaska
 * \date   2010-04-29
 *
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

#include <lwip/genode.h>

extern void create_lwip_plugin();

void __attribute__((constructor)) init_loopback(void)
{
	PDBG("init_loopback()\n");

	/* make sure the libc_lwip plugin has been created */
	create_lwip_plugin();

	lwip_loopback_init();
}
