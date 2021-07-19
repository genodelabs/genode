/*
 * \brief  Replaces lib/smp_processor_id.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/smp.h>

notrace unsigned int debug_smp_processor_id(void)
{
	return 0;
}
