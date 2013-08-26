/*
 * \brief  Lxip plugin creation
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2013-09-04
 *
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

extern void create_lxip_plugin();

void __attribute__((constructor)) init_libc_lxip(void)
{
	PDBG("init_libc_lxip()\n");
	create_lxip_plugin();
}
