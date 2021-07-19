/*
 * \brief  Replaces drivers/base/power/common.c
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

#include <linux/pm_domain.h>

int dev_pm_domain_attach(struct device * dev, bool power_on)
{
	return 0;
}


void dev_pm_domain_detach(struct device * dev, bool power_off) { }
