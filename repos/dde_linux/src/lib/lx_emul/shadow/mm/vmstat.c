/*
 * \brief  Replaces mm/vmstat.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/mm.h>
#include <linux/vmstat.h>


#ifdef CONFIG_SMP
void quiet_vmstat(void) { }

void mod_node_page_state(struct pglist_data *pgdat, enum node_stat_item item, long delta)
{
	lx_emul_trace(__func__);
}
#endif
