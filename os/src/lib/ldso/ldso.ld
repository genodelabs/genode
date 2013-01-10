/*
 * \brief  LDSO specific linkage settings
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2011-04-27
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
SECTIONS
{
	/* set ldso memory location */
	. = 0x30000 - SIZEOF_HEADERS;
}

