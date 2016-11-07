/*
 * \brief  lwip plugin creation
 * \author Christian Prochaska
 * \date   2010-02-12
 *
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern void create_lwip_plugin();

void __attribute__((constructor)) init_libc_lwip(void)
{
	create_lwip_plugin();
}
