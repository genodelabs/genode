/*
 * \brief  Replaces driver/acpi/glue.c
 * \author Josef Soentgen
 * \date   2022-05-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/kobject.h>
#include <linux/acpi.h>

int acpi_platform_notify(struct device *dev, enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}
