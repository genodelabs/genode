/*
 * \brief  Replaces drivers/base/power/main.c
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

#include <linux/pm.h>
#include <../drivers/base/power/power.h>

void device_pm_add(struct device * dev) { }


void device_pm_move_last(struct device * dev) { }


void device_pm_remove(struct device * dev) { }


void device_pm_sleep_init(struct device * dev) { }


void device_pm_check_callbacks(struct device * dev) { }


void device_pm_lock(void) { }


void device_pm_unlock(void) { }
