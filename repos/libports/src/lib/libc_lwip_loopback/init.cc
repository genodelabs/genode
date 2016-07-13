/*
 * \brief  lwip loopback interface initialization
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

#include <base/log.h>

#include <lwip/genode.h>

extern void create_lwip_plugin();

void __attribute__((constructor)) init_loopback(void)
{
	Genode::log(__func__);

	/* make sure the libc_lwip plugin has been created */
	create_lwip_plugin();

	/**
	 * As of lwip-1.4.x this is not needed anymore because lwip
	 * now always creates a loopback device. This plug-in will
	 * be removed in the future but for now keep it around so
	 * we currently do not need to update the other targets that
	 * depend on it.
	 *
	 * lwip_loopback_init();
	 */
}
