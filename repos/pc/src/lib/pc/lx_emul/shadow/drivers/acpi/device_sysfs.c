/*
 * \brief  Replaces driver/acpi/device_sysfs.c
 * \author Josef Soentgen
 * \date   2022-05-06
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/acpi.h>


int acpi_device_uevent_modalias(struct device *dev, struct kobj_uevent_env *event)
{
	lx_emul_trace(__func__);
	return -1;
}


int acpi_device_modalias(struct device *device, char * x, int y)
{
	lx_emul_trace(__func__);
	return -1;
}
