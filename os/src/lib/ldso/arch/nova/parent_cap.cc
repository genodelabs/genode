/*
 * \brief  Parent capability manipulation
 * \author Alexander Boettcher
 * \date   2012-08-13
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <ldso/arch.h>

void Genode::set_parent_cap_arch(void *ptr)
{
	/* Not required, determinig parent cap is done not using any exported
	 * symbols
	 */
}
