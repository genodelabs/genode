/**
 * \brief  Constructor for libc plugin
 * \author Sebastian Sumpf
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

void __attribute__((constructor)) init_libc_plugin(void)
{
	extern void create_libc_plugin();
	create_libc_plugin();
}
