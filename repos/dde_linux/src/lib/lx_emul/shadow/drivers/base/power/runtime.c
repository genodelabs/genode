/*
 * \brief  Replaces drivers/base/power/runtime.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 *
 * We do not support power-management by now, so leave it empty.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/pm_runtime.h>
#include <../drivers/base/power/power.h>

void pm_runtime_init(struct device * dev) { }


int __pm_runtime_resume(struct device * dev,int rpmflags)
{
	return 0;
}


int __pm_runtime_idle(struct device * dev,int rpmflags)
{
	return 0;
}


void pm_runtime_get_suppliers(struct device * dev) { }


int pm_runtime_barrier(struct device * dev)
{
	return 0;
}


void pm_runtime_reinit(struct device * dev) { }


void pm_runtime_put_suppliers(struct device * dev) { }


int pm_generic_runtime_resume(struct device * dev)
{
	return 0;
}


int pm_generic_runtime_suspend(struct device * dev)
{
	return 0;
}


void pm_runtime_clean_up_links(struct device * dev) { }


void pm_runtime_drop_link(struct device_link * link) { }


void pm_runtime_new_link(struct device * dev) { }
