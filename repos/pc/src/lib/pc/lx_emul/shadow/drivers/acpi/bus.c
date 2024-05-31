/*
 * \brief  Replaces driver/acpi/bus.c
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

struct device_driver;

bool acpi_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	lx_emul_trace(__func__);
	return false;
}
